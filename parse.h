#ifndef __PARSE_H
#define __PARSE_H

struct http_request {
	struct tm *time;
	enum {GET, HEAD, UNSUPPORTED} type;
	char *uri;
	int if_modified;
	int mjr;
	int mnr;
};

int parse_request(int, struct http_request *);

#endif
