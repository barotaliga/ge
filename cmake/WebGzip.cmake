# Run by ge_add_web_viewer after the link (cmake -P, in the viewer's folder): a gzip copy of the
# viewer's file NAME beside it (`<file>.gz`, the highest level), which a hub sends to a browser
# that takes gzip (engine/hub/watch.cpp's viewer_file).
file(ARCHIVE_CREATE OUTPUT ${NAME}.gz PATHS ${NAME} FORMAT raw COMPRESSION GZip
     COMPRESSION_LEVEL 9)
