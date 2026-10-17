#include "gtk/study_bible_pdf.h"
#include "backend/editable_edition.h"

#include <cairo-pdf.h>
#include <pango/pangocairo.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <exception>
#include <map>
#include <memory>
#include <sstream>
#include <vector>

namespace {
constexpr double width = 595.28, height = 841.89, margin = 48, bottom = height - 52;
bool contributed(const EditionComparison &v) { return v.edited() || v.commented() || v.pending(); }
std::string key(const EditionComparison &v) {
    return std::to_string(v.reference.book)+"-"+std::to_string(v.reference.chapter)+"-"+std::to_string(v.reference.verse);
}
std::string reference(const EditionComparison &v) {
    return v.bookName+" "+std::to_string(v.reference.chapter)+":"+std::to_string(v.reference.verse);
}
std::string quoted(const std::string &s) {
    std::string result="'";
    for(char c:s) { if(c=='\'' || c=='\\')result+='\\'; result+=c; }
    return result+"'";
}
std::string number(double value) {
    char buffer[G_ASCII_DTOSTR_BUF_SIZE];g_ascii_dtostr(buffer,sizeof buffer,value);return buffer;
}
std::vector<std::string> urls(const std::string &s) {
    std::vector<std::string> result;
    GRegex *regex=g_regex_new("https?://[^\\s<>]+",G_REGEX_DEFAULT,G_REGEX_MATCH_DEFAULT,nullptr);
    GMatchInfo *matches=nullptr;g_regex_match(regex,s.c_str(),G_REGEX_MATCH_DEFAULT,&matches);
    while(g_match_info_matches(matches)) {
        gchar *raw=g_match_info_fetch(matches,0);std::string url=raw;g_free(raw);
        while(!url.empty() && std::string(".,;\r").find(url.back())!=std::string::npos)url.pop_back();
        if(std::find(result.begin(),result.end(),url)==result.end())result.push_back(url);
        g_match_info_next(matches,nullptr);
    }
    g_match_info_free(matches);g_regex_unref(regex);return result;
}
std::string noteTitle(const EditionComparison &v) {
    const auto &s=v.rationale.empty()?v.comment:v.rationale;
    auto end=s.find('\n');
    return end!=std::string::npos && end>0 && end<240?s.substr(0,end):reference(v);
}

class Document {
public:
    Document(cairo_surface_t *surface,const EditableEditionMetadata &metadata,
             const StudyBiblePdfOptions &options):surface(surface),options(options),english(metadata.language=="en") {
        cr=cairo_create(surface);
        // A private font map avoids sharing mutable Pango state with GTK's thread.
        PangoFontMap *map=pango_cairo_font_map_new();
        PangoContext *context=pango_font_map_create_context(map);
        pango_cairo_context_set_resolution(context,72);
        layout=pango_layout_new(context);
        g_object_unref(context);g_object_unref(map);
        pango_layout_set_width(layout,static_cast<int>((width-2*margin)*PANGO_SCALE));
        pango_layout_set_wrap(layout,PANGO_WRAP_WORD_CHAR);
        pango_layout_set_auto_dir(layout,TRUE);
        cairo_pdf_surface_set_metadata(surface,CAIRO_PDF_METADATA_TITLE,metadata.name.c_str());
        cairo_pdf_surface_set_metadata(surface,CAIRO_PDF_METADATA_CREATOR,"Ministerios Elim");
        cairo_pdf_surface_set_metadata(surface,CAIRO_PDF_METADATA_SUBJECT,
            tr("Biblia de estudio con aportes enlazados","Study Bible with linked contributions"));
        runningTitle=metadata.name;
    }
    ~Document(){g_object_unref(layout);cairo_destroy(cr);}
    const char *tr(const char *es,const char *en)const{return english?en:es;}
    void check()const { if(options.cancelled && options.cancelled())throw std::string(tr("Exportación cancelada.","Export cancelled.")); }
    void page() {
        check();footer();cairo_show_page(cr);++pages;y=margin;
        textAt(runningTitle,margin,24,8,false);
    }
    void room(double needed) { if(y+needed>bottom)page(); }
    void destination(const std::string &id) {
        const auto attrs="name="+quoted(id)+" x="+number(margin)+" y="+number(y);
        cairo_tag_begin(cr,CAIRO_TAG_DEST,attrs.c_str());cairo_tag_end(cr,CAIRO_TAG_DEST);
    }
    int outline(const std::string &title,const std::string &dest,int parent=CAIRO_PDF_OUTLINE_ROOT) {
        const auto attrs="dest="+quoted(dest);
        return cairo_pdf_surface_add_outline(surface,parent,title.c_str(),attrs.c_str(),static_cast<cairo_pdf_outline_flags_t>(0));
    }
    void paragraph(const std::string &text,double size=11,bool bold=false,
                   const std::string &link={},bool external=false) {
        check();if(text.empty())return;
        if(!g_utf8_validate(text.c_str(),text.size(),nullptr))throw std::string("Invalid UTF-8 in study Bible content");
        PangoFontDescription *font=pango_font_description_new();
        pango_font_description_set_family(font,"sans-serif");
        pango_font_description_set_absolute_size(font,size*PANGO_SCALE);
        pango_font_description_set_weight(font,bold?PANGO_WEIGHT_BOLD:PANGO_WEIGHT_NORMAL);
        pango_layout_set_font_description(layout,font);pango_font_description_free(font);
        pango_layout_set_text(layout,text.c_str(),text.size());
        const auto addresses=link.empty()?urls(text):std::vector<std::string>{};
        PangoAttrList *attributes=pango_attr_list_new();
        for(const auto &url:addresses){
            size_t pos=0;
            while((pos=text.find(url,pos))!=std::string::npos){
                for(auto *attribute:{pango_attr_foreground_new(0x2666,0x4ccc,0x7aaa),pango_attr_underline_new(PANGO_UNDERLINE_SINGLE)}){
                    attribute->start_index=pos;attribute->end_index=pos+url.size();pango_attr_list_insert(attributes,attribute);
                }
                pos+=url.size();
            }
        }
        pango_layout_set_attributes(layout,attributes);pango_attr_list_unref(attributes);
        std::unique_ptr<PangoLayoutIter,decltype(&pango_layout_iter_free)> iterator(pango_layout_get_iter(layout),pango_layout_iter_free);
        PangoLayoutIter *iter=iterator.get();
        do {
            check();PangoLayoutLine *line=pango_layout_iter_get_line_readonly(iter);
            PangoRectangle logical;pango_layout_iter_get_line_extents(iter,nullptr,&logical);
            const double lineHeight=std::max(size*1.35,logical.height/static_cast<double>(PANGO_SCALE));
            room(lineHeight);
            const double baseline=(pango_layout_iter_get_baseline(iter)-logical.y)/static_cast<double>(PANGO_SCALE);
            if(!link.empty())cairo_tag_begin(cr,CAIRO_TAG_LINK,((external?"uri=":"dest=")+quoted(link)).c_str());
            cairo_set_source_rgb(cr,link.empty()?0.13:0.15,link.empty()?0.13:0.30,link.empty()?0.13:0.48);
            cairo_move_to(cr,margin+logical.x/static_cast<double>(PANGO_SCALE),y+baseline);
            pango_cairo_show_layout_line(cr,line);
            if(!link.empty())cairo_tag_end(cr,CAIRO_TAG_LINK);
            for(const auto &url:addresses) {
                size_t pos=0;
                while((pos=text.find(url,pos))!=std::string::npos) {
                    const int from=std::max(static_cast<int>(pos),line->start_index);
                    const int to=std::min(static_cast<int>(pos+url.size()),line->start_index+line->length);
                    if(from<to) {
                        int *ranges=nullptr,count=0;pango_layout_line_get_x_ranges(line,from,to,&ranges,&count);
                        gchar *ascii=g_uri_escape_string(url.c_str(),":/?#[]@!$&'()*+,;=%",FALSE);
                        for(int i=0;i<count;++i) {
                            const double x=margin+ranges[i*2]/static_cast<double>(PANGO_SCALE);
                            const double w=(ranges[i*2+1]-ranges[i*2])/static_cast<double>(PANGO_SCALE);
                            const auto attrs="uri="+quoted(ascii)+" rect=["+number(x)+" "+number(y)+" "+number(w)+" "+number(lineHeight)+"]";
                            cairo_tag_begin(cr,CAIRO_TAG_LINK,attrs.c_str());cairo_tag_end(cr,CAIRO_TAG_LINK);
                        }
                        g_free(ascii);g_free(ranges);
                    }
                    pos+=url.size();
                }
            }
            y+=lineHeight;
        } while(pango_layout_iter_next_line(iter));
        y+=5;
    }
    int heading(const std::string &title,const std::string &dest={},int parent=0) {
        room(70);int item=0;if(!dest.empty()){destination(dest);item=outline(title,dest,parent);}
        paragraph(title,18,true);y+=5;
        return item;
    }
    void finish(){footer();cairo_show_page(cr);if(cairo_status(cr)!=CAIRO_STATUS_SUCCESS)throw std::string(cairo_status_to_string(cairo_status(cr)));}
    int pages=1;
private:
    void textAt(const std::string &text,double x,double top,double size,bool bold) {
        // Separate layout: page headers must not invalidate paragraph iterators.
        PangoLayout *label=pango_layout_new(pango_layout_get_context(layout));
        PangoFontDescription *font=pango_font_description_from_string(bold?"sans-serif Bold":"sans-serif");
        pango_font_description_set_absolute_size(font,size*PANGO_SCALE);
        pango_layout_set_font_description(label,font);pango_font_description_free(font);
        pango_layout_set_width(label,(width-2*margin)*PANGO_SCALE);
        pango_layout_set_ellipsize(label,PANGO_ELLIPSIZE_END);
        pango_layout_set_text(label,text.c_str(),-1);
        cairo_set_source_rgb(cr,.4,.4,.4);cairo_move_to(cr,x,top);pango_cairo_show_layout(cr,label);g_object_unref(label);
    }
    void footer(){textAt(std::to_string(pages),width/2, height-30,8,false);}
    cairo_surface_t *surface; cairo_t *cr; PangoLayout *layout;
    const StudyBiblePdfOptions &options; bool english; double y=margin;
    std::string runningTitle;
};

bool readBookComments(const std::string &path,std::map<int,std::string> &comments,std::string &error) {
    sqlite3 *raw=nullptr;
    if(sqlite3_open_v2(path.c_str(),&raw,SQLITE_OPEN_READONLY,nullptr)!=SQLITE_OK) {
        error=raw?sqlite3_errmsg(raw):"Cannot open edition";if(raw)sqlite3_close(raw);return false;
    }
    std::unique_ptr<sqlite3,decltype(&sqlite3_close)> db(raw,sqlite3_close);
    sqlite3_stmt *stmt=nullptr;
    if(sqlite3_prepare_v2(raw,"SELECT 1 FROM sqlite_master WHERE type='table' AND name='book_comments'",-1,&stmt,nullptr)!=SQLITE_OK){error=sqlite3_errmsg(raw);return false;}
    const bool exists=sqlite3_step(stmt)==SQLITE_ROW;sqlite3_finalize(stmt);
    if(!exists)return true;
    if(sqlite3_prepare_v2(raw,"SELECT book_id,comment FROM book_comments",-1,&stmt,nullptr)!=SQLITE_OK){error=sqlite3_errmsg(raw);return false;}
    std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)> guard(stmt,sqlite3_finalize);
    int rc;
    while((rc=sqlite3_step(stmt))==SQLITE_ROW) {
        const char *text=reinterpret_cast<const char*>(sqlite3_column_text(stmt,1));
        if(text && *text)comments[sqlite3_column_int(stmt,0)]=text;
    }
    if(rc!=SQLITE_DONE){error=sqlite3_errmsg(raw);return false;}return true;
}
cairo_status_t writePdf(void *file,const unsigned char *data,unsigned int length) {
    return fwrite(data,1,length,static_cast<FILE*>(file))==length?CAIRO_STATUS_SUCCESS:CAIRO_STATUS_WRITE_ERROR;
}
}

bool exportStudyBiblePdf(const std::string &path,const std::string &output,
                        const StudyBiblePdfOptions &options,StudyBiblePdfStats &stats,std::string &error)
{
    stats={};error.clear();
    if(path.empty() || output.empty()){error="Debes indicar la Biblia y el archivo PDF de destino.";return false;}
    GStatBuf inputInfo{},outputInfo{};
    gchar *sourceName=g_canonicalize_filename(path.c_str(),nullptr),*targetName=g_canonicalize_filename(output.c_str(),nullptr);
    const bool same=
#ifdef G_OS_WIN32
        !g_ascii_strcasecmp(sourceName,targetName) ||
#else
        !g_strcmp0(sourceName,targetName) ||
#endif
        (g_stat(path.c_str(),&inputInfo)==0 && g_stat(output.c_str(),&outputInfo)==0 &&
         inputInfo.st_ino!=0 && inputInfo.st_dev==outputInfo.st_dev && inputInfo.st_ino==outputInfo.st_ino);
    g_free(sourceName);g_free(targetName);
    if(same){error="El destino no puede ser el proyecto de origen.";return false;}
    EditableEditionMetadata metadata;std::vector<EditionComparison> verses;std::map<int,std::string> comments;
    if(!EditableEdition::comparisons(path,metadata,verses,error) || !readBookComments(path,comments,error))return false;
    if(verses.empty()){error="La Biblia no contiene versículos.";return false;}
    if(options.contributionsOnly)verses.erase(std::remove_if(verses.begin(),verses.end(),
        [](const EditionComparison &v){return !contributed(v);}),verses.end());
    if(verses.empty()){error="No hay versículos con aportes para exportar.";return false;}
    std::string temp=output+".XXXXXX";
    int fd=g_mkstemp(&temp[0]);
    if(fd<0){error=g_strerror(errno);return false;}
    FILE *file=fdopen(fd,"wb");
    if(!file){g_close(fd,nullptr);g_unlink(temp.c_str());error=g_strerror(errno);return false;}
    cairo_surface_t *surface=cairo_pdf_surface_create_for_stream(writePdf,file,width,height);
    bool success=false;
    try {
        Document doc(surface,metadata,options);
        std::vector<const EditionComparison*> notes;std::vector<const EditionComparison*> books;
        std::map<std::string,int> noteNumbers;
        int previousBook=-1;
        for(const auto &v:verses) {
            if(v.reference.book!=previousBook){books.push_back(&v);previousBook=v.reference.book;}
            if(contributed(v)){notes.push_back(&v);noteNumbers[key(v)]=notes.size();}
        }
        doc.heading(metadata.name,"cover");
        doc.paragraph(doc.tr("Biblia de estudio","Study Bible"),14);
        doc.paragraph(std::string(doc.tr("Basada en: ","Based on: "))+metadata.sourceName);
        doc.paragraph(std::string(doc.tr("Licencia de la Biblia base: ","Base Bible license: "))+metadata.sourceLicense,10);
        doc.paragraph(metadata.sourceAttribution,10);
        doc.paragraph(options.contributionsOnly?doc.tr("Selección: versículos con aportes.","Selection: verses with contributions."):
            doc.tr("Texto bíblico completo con aportes de esta edición.","Complete Bible text with contributions from this edition."));
        doc.paragraph(doc.tr("Pulsa «Aporte» junto a un versículo para leer su comentario y sus fuentes. «Volver al versículo» permite continuar la lectura. También puedes usar los índices y los marcadores del PDF.",
            "Select ‘Contribution’ beside a verse to read its commentary and sources. ‘Back to verse’ returns to your reading. You can also use the indexes and PDF bookmarks."));
        doc.paragraph(doc.tr("Los aportes reflejan el trabajo editorial guardado en este proyecto; las propuestas pendientes se distinguen del texto bíblico.",
            "Contributions reflect the editorial work saved in this project; pending proposals are distinguished from the Bible text."),10);
        doc.paragraph(doc.tr("Índice de libros","Book index"),12,true,"books");
        doc.paragraph(doc.tr("Índice de aportes","Contribution index"),12,true,"contributions");
        doc.page();doc.heading(doc.tr("Índice de libros","Book index"),"books");
        for(const auto *v:books)doc.paragraph(v->bookName,11,false,"book-"+std::to_string(v->reference.book));
        doc.page();doc.heading(doc.tr("Índice de aportes","Contribution index"),"contributions");
        if(notes.empty())doc.paragraph(doc.tr("Esta edición aún no tiene aportes por versículo.","This edition has no verse contributions yet."));
        for(const auto *v:notes)doc.paragraph(reference(*v)+" · "+noteTitle(*v),11,false,"note-"+key(*v));
        previousBook=-1;int previousChapter=-1,bookOutline=0;
        for(const auto &v:verses) {
            doc.check();
            if(v.reference.book!=previousBook) {
                doc.page();const auto id="book-"+std::to_string(v.reference.book);
                bookOutline=doc.heading(v.bookName,id);
                const auto intro=comments.find(v.reference.book);
                if(intro!=comments.end())doc.paragraph(intro->second);
                previousBook=v.reference.book;previousChapter=-1;
            }
            if(v.reference.chapter!=previousChapter) {
                doc.room(70);const auto id="chapter-"+std::to_string(v.reference.book)+"-"+std::to_string(v.reference.chapter);
                doc.destination(id);doc.outline(v.bookName+" "+std::to_string(v.reference.chapter),id,bookOutline);
                doc.paragraph(std::string(doc.tr("Capítulo ","Chapter "))+std::to_string(v.reference.chapter),13,true);
                previousChapter=v.reference.chapter;
            }
            doc.room(40);if(contributed(v))doc.destination("verse-"+key(v));
            doc.paragraph(std::to_string(v.reference.verse)+"  "+v.currentText);
            if(contributed(v))doc.paragraph(std::string(doc.tr("Aporte ","Contribution "))+std::to_string(noteNumbers[key(v)]),9,true,"note-"+key(v));
        }
        if(!notes.empty()) {
            doc.page();doc.heading(doc.tr("Aportes de esta edición","Contributions from this edition"),"notes");
            const int noteOutline=doc.outline(doc.tr("Comentarios y cambios","Commentary and changes"),"notes");
            for(const auto *v:notes) {
                doc.room(130);const auto id="note-"+key(*v);doc.destination(id);
                doc.outline(reference(*v)+" · "+noteTitle(*v),id,noteOutline);
                doc.paragraph(reference(*v),14,true);
                const std::string body=v->rationale.empty()?v->comment:v->rationale;
                const auto title=noteTitle(*v);
                if(title!=reference(*v)) {
                    doc.paragraph(title,12,true);
                    const auto rest=body.substr(body.find('\n')+1);
                    const auto leadEnd=rest.find('\n');
                    doc.paragraph(rest.substr(0,leadEnd),11,true);
                    if(leadEnd!=std::string::npos)doc.paragraph(rest.substr(leadEnd+1));
                }else if(!body.empty())doc.paragraph(body);
                if(v->edited()) {
                    doc.paragraph(doc.tr("Cambio de traducción","Translation change"),11,true);
                    doc.paragraph(metadata.sourceName+": "+v->baseText,10);
                    doc.paragraph(metadata.name+": "+v->currentText,10);
                }
                if(v->pending() && v->proposal!=v->currentText) {
                    doc.paragraph(doc.tr("Propuesta pendiente de revisión","Proposal awaiting review"),11,true);
                    doc.paragraph(v->proposal,10);
                }
                const auto visible=urls(body);std::vector<std::string> extra;
                for(const auto &url:urls(v->comment+"\n"+v->evidence))
                    if(std::find(visible.begin(),visible.end(),url)==visible.end())extra.push_back(url);
                if(!extra.empty()) {
                    doc.paragraph(doc.tr("Fuentes","Sources"),10,true);
                    for(const auto &url:extra)doc.paragraph(url,9);
                }
                doc.paragraph(doc.tr("Volver al versículo","Back to verse"),10,true,"verse-"+key(*v));
                doc.paragraph(doc.tr("Volver al índice de aportes","Back to contribution index"),9,false,"contributions");
            }
        }
        doc.finish();doc.check();
        stats={static_cast<int>(books.size()),static_cast<int>(verses.size()),static_cast<int>(notes.size()),doc.pages};
        success=true;
    }catch(const std::string &message){error=message;}
    catch(const std::exception &exception){error=exception.what();}
    cairo_surface_finish(surface);
    if(cairo_surface_status(surface)!=CAIRO_STATUS_SUCCESS){error=cairo_status_to_string(cairo_surface_status(surface));success=false;}
    cairo_surface_destroy(surface);
    if(fclose(file)!=0){error=g_strerror(errno);success=false;}
    if(success && options.cancelled && options.cancelled()){error="Exportación cancelada.";success=false;}
    if(success) {
        GFile *from=g_file_new_for_path(temp.c_str()),*to=g_file_new_for_path(output.c_str());GError *failure=nullptr;
        success=g_file_move(from,to,G_FILE_COPY_OVERWRITE,nullptr,nullptr,nullptr,&failure);
        if(!success){error=failure?failure->message:"Cannot save PDF";g_clear_error(&failure);}
        g_object_unref(from);g_object_unref(to);
    }
    if(!success){g_unlink(temp.c_str());stats={};}return success;
}
