FROM ghcr.io/userver-framework/docker-userver-build-base:v1a AS builder

WORKDIR /lavka

COPY . ./

RUN ls -a

RUN make build-release

FROM builder AS runner

EXPOSE 8080

CMD ["make", "service-start-manually-release"]
