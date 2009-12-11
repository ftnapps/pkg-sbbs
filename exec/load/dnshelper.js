// $Id: dnshelper.js,v 1.4 2005/05/09 23:26:15 rswindell Exp $

var host;
if((host=resolve_host(argv[0]))==null)
	false;	// Work-around for bug in v3.12a (segfault when decoding "null")
else
	host;