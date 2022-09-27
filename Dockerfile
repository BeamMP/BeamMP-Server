FROM lionkor/alpine-static-cpp:latest

RUN apk update && apk --no-cache add python3 git lua zlib-static openssl curl rapidjson curl-dev wget
RUN apk --no-cache add openssl-libs-static curl-static

RUN wget "https://www.lua.org/ftp/lua-5.4.4.tar.gz"; tar xzvf lua-5.4.4.tar.gz; mv lua-5.4.4 /lua; rm lua-5.4.4.tar.gz

RUN cd /lua; make all -j; cd ..

