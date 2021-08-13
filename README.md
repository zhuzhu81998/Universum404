# Universum404
## General
 A simple multi-threaded http web server written in pure C for Windows.  
 Compiled successfully for Windows 10 x64 with vc++

## TODO (updated at 08/13/2021)
 1. (half done) finish the function of interpreting/creating http headers
 2. add cookie support
 3. add ssl support

## What can this server already do?
 - support both ipv4 and ipv6
 - process requests multi-threaded
 - response with requested files (including binary files)
 - support cgi scripts (for now only php with GET method)
 - check mime type with a mime.types file

## Known problems
 1. memory leak somewhere (the memory usage increases with every request)