#include <stdlib.h>
#include <stdio.h>

#include "urlparse.h"
#include "httpfetch.h"

int main(int argc, char **argv){
	struct urllist *ul;

	ul=urlparse();
	fetch_urls(ul,5);

	return 0;
}
