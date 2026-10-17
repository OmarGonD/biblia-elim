#include "gtk/study_bible_pdf.h"
#include <iostream>

int main(int argc,char **argv)
{
    if(argc<3 || argc>4 || (argc==4 && std::string(argv[3])!="--contributions-only")) {
        std::cerr<<"Usage: biblia-study-pdf EDITION.sqlite OUTPUT.pdf [--contributions-only]\n";return 2;
    }
    StudyBiblePdfOptions options;options.contributionsOnly=argc==4;
    StudyBiblePdfStats stats;std::string error;
    if(!exportStudyBiblePdf(argv[1],argv[2],options,stats,error)){std::cerr<<error<<'\n';return 1;}
    std::cout<<"books="<<stats.books<<" verses="<<stats.verses<<" contributions="<<stats.contributions<<" pages="<<stats.pages<<'\n';
}
