#!/bin/bash

groupadd -f readonly
groupadd -f staff

useradd -M -s /sbin/nologin -G readonly member 2>/dev/null || true
useradd -M -s /sbin/nologin -G staff contributor 2>/dev/null || true
useradd -M -s /sbin/nologin -G staff librarian 2>/dev/null || true

echo "member:member123" | chpasswd
echo "contributor:contrib456" | chpasswd
echo "librarian:lib789" | chpasswd

(echo "member123"; echo "member123") | smbpasswd -a -s member
(echo "contrib456"; echo "contrib456") | smbpasswd -a -s contributor
(echo "lib789"; echo "lib789") | smbpasswd -a -s librarian

smbpasswd -e member
smbpasswd -e contributor
smbpasswd -e librarian

chmod 775 /libraryit/ebooks
chmod 775 /libraryit/papers
chmod 750 /libraryit/sourcecode
chmod 775 /libraryit/docs

chown root:staff /libraryit/ebooks
chown root:staff /libraryit/papers
chown root:staff /libraryit/sourcecode
chown librarian:staff /libraryit/docs

exec smbd --foreground --no-process-group "$@"
