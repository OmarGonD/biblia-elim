#include "backend/strong_id.h"
#include <cctype>
bool parseStrongId(const std::string &v, StrongId &out) {
 if (v.size()<2 || (v[0]!='H' && v[0]!='G')) return false; int n=0;
 for (std::size_t i=1;i<v.size();++i) { if(!std::isdigit((unsigned char)v[i])) return false; n=n*10+v[i]-'0'; }
 if(n<=0) return false; out.language=v[0]=='H'?StrongLanguage::Hebrew:StrongLanguage::Greek; out.number=n; return true;
}
std::vector<StrongId> parseStrongIds(const std::string &v) { std::vector<StrongId> r; std::size_t b=0; while(b<=v.size()){std::size_t e=v.find(',',b); StrongId id; std::string p=v.substr(b,e==std::string::npos?e:e-b); if(!p.empty()&&parseStrongId(p,id))r.push_back(id); if(e==std::string::npos)break; b=e+1;} return r; }
std::string formatStrongId(const StrongId &id) { return std::string(1,id.language==StrongLanguage::Hebrew?'H':'G')+std::to_string(id.number); }
