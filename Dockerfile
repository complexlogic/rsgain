FROM debian

ARG VERSION=2.0 \
    ARCH=amd64

RUN apt-get update && \
    apt-get install -y --no-install-recommends curl ca-certificates openssl libavcodec58 libavutil56 libswresample3 libinih1 libavformat58 libtag1v5 libebur128-1 && \
    curl -sSL -o /tmp/rsgain.deb "https://github.com/complexlogic/rsgain/releases/download/v${VERSION}/rsgain_${VERSION}_${ARCH}.deb" && \
    dpkg -i /tmp/rsgain.deb && \
    rm -rf /var/lib/apt/lists/* /tmp/rsgain.deb

ENTRYPOINT /usr/bin/rsgain
