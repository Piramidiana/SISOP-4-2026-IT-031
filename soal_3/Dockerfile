FROM ubuntu:latest

RUN apt-get update && apt-get install -y samba && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs /logs

COPY smb.conf /etc/samba/smb.conf
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

EXPOSE 445 139

ENTRYPOINT ["/entrypoint.sh"]
