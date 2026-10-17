#include "gtk/study_bible_pdf.h"
#include "backend/editable_edition.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <sqlite3.h>
#ifdef HAVE_TEST_POPPLER
#include <poppler.h>
#endif
#include <cstdio>
#include <string>
#include <vector>

namespace {
int failures=0,checks=0;
void check(bool value,const char *message){++checks;if(!value){++failures;g_printerr("FAIL: %s\n",message);}}
std::string contents(const std::string &path){gchar *data=nullptr;gsize size=0;g_file_get_contents(path.c_str(),&data,&size,nullptr);std::string result(data?data:"",size);g_free(data);return result;}
void execute(const std::string &path,const char *sql){sqlite3 *db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK,"open fixture");check(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)==SQLITE_OK,"update fixture");sqlite3_close(db);}
bool noTemps(const std::string &dir){GDir *d=g_dir_open(dir.c_str(),0,nullptr);bool clean=true;const char *name;while(d && (name=g_dir_read_name(d)))if(g_str_has_prefix(name,"cancel.pdf.") || g_str_has_prefix(name,"existing.pdf."))clean=false;if(d)g_dir_close(d);return clean;}
#ifdef HAVE_TEST_POPPLER
std::string inspect(const std::string &path,const StudyBiblePdfStats &stats,bool only,bool english=false)
{
    gchar *uri=g_filename_to_uri(path.c_str(),nullptr,nullptr);GError *error=nullptr;
    PopplerDocument *doc=poppler_document_new_from_file(uri,nullptr,&error);g_free(uri);
    check(doc!=nullptr,error?error->message:"PDF opens in an independent reader");g_clear_error(&error);if(!doc)return {};
    const int pages=poppler_document_get_n_pages(doc);check(pages==stats.pages,"reported page count equals real PDF pages");
    int internal=0,external=0;bool apostrophe=false,forward=false,back=false;
    std::string text;
    for(int i=0;i<pages;++i){
        PopplerPage *page=poppler_document_get_page(doc,i);gchar *part=poppler_page_get_text(page);text+=part?part:"";text+='\n';g_free(part);
        GList *links=poppler_page_get_link_mapping(page);
        for(GList *l=links;l;l=l->next){
            auto *link=static_cast<PopplerLinkMapping*>(l->data);
            check(link->area.x2>link->area.x1 && link->area.y2>link->area.y1 &&
                  link->area.x1>=0 && link->area.y1>=0 && link->area.x2<596 && link->area.y2<843,
                  "link has a visible, bounded clickable rectangle");
            if(link->action->type==POPPLER_ACTION_GOTO_DEST){
                ++internal;PopplerDest *dest=link->action->goto_dest.dest,*resolved=nullptr;
                if(dest && dest->type==POPPLER_DEST_NAMED){
                    const std::string name=dest->named_dest;
                    if(name=="note-1-1-1")forward=true;if(name=="verse-1-1-1")back=true;
                    resolved=poppler_document_find_dest(doc,dest->named_dest);dest=resolved;
                }
                check(dest && dest->page_num>=1 && dest->page_num<=pages,"every internal link resolves to a real PDF page");
                if(resolved)poppler_dest_free(resolved);
            }else if(link->action->type==POPPLER_ACTION_URI){
                ++external;if(std::string(link->action->uri.uri)=="https://example.org/author's-study?q=one&b=two")apostrophe=true;
            }
        }
        poppler_page_free_link_mapping(links);g_object_unref(page);
    }
    // Two cover links, one entry per book, and index/marker/back/index links per contribution.
    check(internal>=2+stats.books+4*stats.contributions && external>=2,"PDF contains both internal navigation and external sources");
    check(forward && back,"contribution and verse have links in both directions");
    check(apostrophe,"quoted source URLs round-trip through Cairo link attributes");
    for(const char *name:{"book-1","chapter-1-1","verse-1-1-1","note-1-1-1","contributions"}){
        PopplerDest *dest=poppler_document_find_dest(doc,name);check(dest!=nullptr,"named destination can be opened independently");if(dest)poppler_dest_free(dest);
    }
    PopplerIndexIter *index=poppler_index_iter_new(doc);check(index!=nullptr,"PDF has a document outline");if(index)poppler_index_iter_free(index);
    check(text.find("INTRODUCCIÓN DEL LIBRO")!=std::string::npos,"book commentary is included");
    check(text.find("FINAL-LONG-NOTE")!=std::string::npos,"long commentary survives page breaks to its final paragraph");
    check(text.find("áéíóú")!=std::string::npos && text.find("τετέλεσται")!=std::string::npos,"Spanish accents and Greek remain searchable text");
    check(text.find("Public Domain")!=std::string::npos && text.find("ATRIBUCIÓN ORIGINAL")!=std::string::npos,"base license and attribution are retained");
    check(text.find("BASE ORIGINAL")!=std::string::npos && text.find("TEXTO EDITADO")!=std::string::npos,"actual text changes include base/current comparison");
    check(text.find(english?"Proposal awaiting review":"Propuesta pendiente de revisión")!=std::string::npos && text.find("SOLO PROPUESTA")!=std::string::npos,"unapplied proposals are labelled as pending");
    check((text.find("VERSO SIN APORTES")!=std::string::npos)==!only,"selection excludes ordinary verses while full Bible retains them");
    g_object_unref(doc);return text;
}
#endif
}

int main()
{
    gchar *tmp=g_dir_make_tmp("study-pdf-test-XXXXXX",nullptr);if(!tmp)return 1;
    const std::string dir=tmp,path=dir+"/edition.sqlite",pdf=dir+"/full.pdf",selection=dir+"/selection.pdf";g_free(tmp);
    EditableEditionMetadata metadata;metadata.projectId="study";metadata.name="Estudio áéíóú";metadata.language="es";
    metadata.versification="kjv";metadata.sourceModuleId="SpaRV";metadata.sourceName="Reina-Valera 1909";
    metadata.sourceLicense="Public Domain";metadata.sourceAttribution="ATRIBUCIÓN ORIGINAL";
    std::vector<SqliteImportBook> books={{1,1,1,"Gen","Génesis","Gn"},{2,1,2,"Exod","Éxodo","Ex"}};
    std::vector<SqliteImportVerse> verses={{{1,1,1,1},"En el principio creó Dios."},{{1,1,1,2},"BASE ORIGINAL"},{{1,2,1,1},"VERSO SIN APORTES"}};
    std::string error;check(EditableEdition::create(path,metadata,books,verses,error),error.c_str());
    auto edition=EditableEdition::open(path,error);check(edition!=nullptr,"open study fixture");if(!edition)return 1;
    std::string note="Título del comentario\nUna bajada breve.\n\nHebreo אֵת y griego τετέλεσται; áéíóú.\n";
    for(int i=0;i<100;++i)note+="\nPárrafo "+std::to_string(i)+": Explicación del contexto y de la función gramatical de la expresión.\n";
    note+="\nFINAL-LONG-NOTE\n\nFuente: Un autor de prueba.\nhttps://example.org/author's-study?q=one&b=two";
    check(edition->setVerseComment({1,1,1,1},note,error),"save long multilingual contribution");
    check(edition->setBookComment(1,"INTRODUCCIÓN DEL LIBRO",error),"save book commentary");
    check(edition->setVerseText({1,1,1,2},"TEXTO EDITADO",error),"edit second verse");
    EditionDecision decision;decision.reference={1,1,1,2};decision.proposal="SOLO PROPUESTA";decision.rationale="Nota editorial\nLa traducción y su contexto.\n\nExplicación de la propuesta.";
    decision.evidence="https://example.org/second-source";decision.status="Pendiente de revisión";
    check(edition->setDecision(decision,error),"save an unapplied proposal");edition.reset();
    const auto original=contents(path);StudyBiblePdfOptions options;StudyBiblePdfStats stats;
    check(exportStudyBiblePdf(path,pdf,options,stats,error),error.c_str());
    check(stats.books==2 && stats.verses==3 && stats.contributions==2 && stats.pages>6,"complete PDF accounts for every book, verse, note and paginated commentary");
    check(contents(pdf).find("%PDF-")==0,"export is an actual PDF");
#ifdef HAVE_TEST_POPPLER
    inspect(pdf,stats,false);
#endif
    options.contributionsOnly=true;
    check(exportStudyBiblePdf(path,selection,options,stats,error),error.c_str());
    check(stats.books==1 && stats.verses==2 && stats.contributions==2,"contribution scope includes changed and commented verses once");
#ifdef HAVE_TEST_POPPLER
    inspect(selection,stats,true);
#endif
    check(contents(path)==original,"export does not change or migrate the source database");
    check(!exportStudyBiblePdf(path,path,{},stats,error) && contents(path)==original,"source cannot be replaced by its PDF");
    const std::string existing=dir+"/existing.pdf";g_file_set_contents(existing.c_str(),"KEEP EXISTING",-1,nullptr);
    options={};int checksBeforeCancel=0;options.cancelled=[&]{return ++checksBeforeCancel>20;};
    check(!exportStudyBiblePdf(path,existing,options,stats,error) && contents(existing)=="KEEP EXISTING" && noTemps(dir),"cancellation mid-render preserves previous PDF and removes temporary file");
    check(stats.pages==0,"failed exports do not report partial success statistics");
    options.cancelled=[] {return true;};
    check(!exportStudyBiblePdf(path,dir+"/cancel.pdf",options,stats,error) && !g_file_test((dir+"/cancel.pdf").c_str(),G_FILE_TEST_EXISTS) && noTemps(dir),"early cancellation leaves no partial PDF");
    check(!exportStudyBiblePdf(path,dir+"/missing/out.pdf",{},stats,error),"unwritable destination reports an error");
    // Legacy projects remain read-only, including missing optional tables.
    execute(path,"DROP TABLE edition_base_verses; DROP TABLE book_comments; UPDATE metadata SET value='en' WHERE key='language';");
    const auto legacy=contents(path);
    check(exportStudyBiblePdf(path,dir+"/legacy.pdf",{},stats,error),error.c_str());
    check(contents(path)==legacy,"legacy export never adds missing tables");
#ifdef HAVE_TEST_POPPLER
    gchar *uri=g_filename_to_uri((dir+"/legacy.pdf").c_str(),nullptr,nullptr);PopplerDocument *doc=poppler_document_new_from_file(uri,nullptr,nullptr);g_free(uri);
    PopplerPage *cover=doc?poppler_document_get_page(doc,0):nullptr;gchar *coverText=cover?poppler_page_get_text(cover):nullptr;
    check(coverText && std::string(coverText).find("Study Bible")!=std::string::npos,"English editions export English navigation and labels");
    g_free(coverText);if(cover)g_object_unref(cover);if(doc)g_object_unref(doc);
#endif
    execute(path,"DELETE FROM verse_comments; DELETE FROM editorial_decisions; DELETE FROM verse_revisions;");
    options={};options.contributionsOnly=true;
    check(!exportStudyBiblePdf(path,dir+"/empty.pdf",options,stats,error) && !g_file_test((dir+"/empty.pdf").c_str(),G_FILE_TEST_EXISTS),"empty contribution selection is explained without creating a PDF");
    GDir *d=g_dir_open(dir.c_str(),0,nullptr);const char *name;while((name=g_dir_read_name(d)))g_unlink((dir+"/"+name).c_str());g_dir_close(d);g_rmdir(dir.c_str());
    g_print("study_bible_pdf checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
