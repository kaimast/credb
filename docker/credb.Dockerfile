FROM credb-dep

ARG sgx_mode
ARG buildtype

COPY . /credb
WORKDIR /credb
RUN ./docker/setup-credb.sh
WORKDIR /credb/build
