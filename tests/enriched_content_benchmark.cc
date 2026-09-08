#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
#include "backend/sqlite/sqlite_bible_backend.h"
static void run(SqliteBibleBackend &b,const char *m,BibleReference r,const char *label){std::vector<long long> v; for(int k=0;k<10;++k){auto s=std::chrono::steady_clock::now(); for(int i=0;i<1000;++i)(void)b.getVerseContent(m,r); v.push_back(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-s).count());} std::sort(v.begin(),v.end()); std::printf("%s median_us=%lld min_us=%lld max_us=%lld\n",label,v[5],v.front(),v.back());}
int main(int argc,char **argv){if(argc!=2)return 2; SqliteBibleBackend b(argv[1]); if(b.listModules().empty())return 1; const char *m=b.listModules().front().id.c_str(); run(b,m,{1,1,1,1},"plain"); run(b,m,{2,43,3,16},"rich"); return 0;}
