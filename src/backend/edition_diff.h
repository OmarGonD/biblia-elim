#ifndef BIBLIA_ELIM_EDITION_DIFF_H
#define BIBLIA_ELIM_EDITION_DIFF_H
#include <glib.h>
#include <string>
#include <utility>

/* Highlight the changed interval, preserving common UTF-8 prefix/suffix.
 * Escape every segment before adding markup; neither text can inject HTML. */
inline std::pair<std::string,std::string> editionDifferenceMarkup(
 const std::string &base, const std::string &current)
{
 std::size_t prefix=0;
 while(prefix<base.size() && prefix<current.size() && base[prefix]==current[prefix]) ++prefix;
 while(prefix>0 && ((prefix<base.size() && (static_cast<unsigned char>(base[prefix])&0xc0)==0x80) ||
                   (prefix<current.size() && (static_cast<unsigned char>(current[prefix])&0xc0)==0x80))) --prefix;
 std::size_t suffix=0;
 while(suffix<base.size()-prefix && suffix<current.size()-prefix &&
       base[base.size()-suffix-1]==current[current.size()-suffix-1]) ++suffix;
 while(suffix>0 && (((static_cast<unsigned char>(base[base.size()-suffix])&0xc0)==0x80) ||
                    ((static_cast<unsigned char>(current[current.size()-suffix])&0xc0)==0x80))) --suffix;
 auto escape=[](const std::string &v){gchar*p=g_markup_escape_text(v.data(),v.size());std::string r=p;g_free(p);return r;};
 auto mark=[&](const std::string &v){
  const auto length=v.size()-prefix-suffix;
  return escape(v.substr(0,prefix)) + (length?"<b><u>"+escape(v.substr(prefix,length))+"</u></b>":"") + escape(v.substr(v.size()-suffix));
 };
 return {mark(base),mark(current)};
}
#endif
