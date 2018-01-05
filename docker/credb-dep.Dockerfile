FROM ubuntu:17.10

COPY docker/setup-dep.sh /dep/setup-dep.sh
COPY docker/download_prebuilt.sh /dep/download_prebuilt.sh
RUN /dep/setup-dep.sh
