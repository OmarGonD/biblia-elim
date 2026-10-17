FROM fedora:45

VOLUME /source

# Installs dependencies once and for all
RUN dnf -y update --refresh && \
    dnf -y install autoconf automake cmake git fpc gettext glib2-devel itstool json-glib-devel libxslt libX11-devel make subversion yelp-tools zip && \
    dnf -y install mingw32-biblesync mingw32-curl mingw32-gcc mingw32-gcc-c++ mingw32-json-glib mingw32-libgnurx mingw32-gettext mingw32-gtk4 mingw32-icu mingw32-libsoup3 mingw32-libtiff mingw32-sqlite mingw32-minizip mingw32-nsis mingw32-xz && \
    dnf -y install mingw64-biblesync mingw64-curl mingw64-gcc mingw64-gcc-c++ mingw64-json-glib mingw64-libgnurx mingw64-gettext mingw64-gtk4 mingw64-icu mingw64-libsoup3 mingw64-libtiff mingw64-sqlite mingw64-minizip mingw64-xz && \
    dnf clean all

ADD build_sword.sh /build_sword.sh

RUN /bin/bash /build_sword.sh

ENTRYPOINT ["/bin/bash", "/source/win32/xc-xiphos-win.sh"]

CMD ["-win32", "-win64"]
