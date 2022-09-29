FROM lionkor/alpine-static-cpp:latest

ARG LUA_V=5.3.6

RUN apk update && apk --no-cache add python3 git lua zlib-static openssl curl rapidjson curl-dev wget readline-static
RUN apk --no-cache add openssl-libs-static curl-static readline-dev

RUN wget "https://www.lua.org/ftp/lua-${LUA_V}.tar.gz"; tar xzvf lua-${LUA_V}.tar.gz; mv "lua-${LUA_V}" /lua; rm lua-${LUA_V}.tar.gz

RUN cd /lua; make all -j linux; cd ..

