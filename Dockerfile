FROM debian:bookworm

ARG VERSION=3.5.3 \
    ARCH=amd64

RUN apt-get update && \
    apt-get install -y --no-install-recommends curl ca-certificates openssl && \
    curl -sSL -o /tmp/rsgain.deb "https://github.com/complexlogic/rsgain/releases/download/v${VERSION}/rsgain_${VERSION}_${ARCH}.deb" && \
    apt install -y /tmp/rsgain.deb && \
    rm -rf /var/lib/apt/lists/* /tmp/rsgain.deb

ENTRYPOINT ["/usr/bin/rsgain"]
