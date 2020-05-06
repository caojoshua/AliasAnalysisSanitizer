#include <stdio.h>
#include <string.h>

struct S {
	S(char* desc) : desc(desc) {}
	S& operator=(const S& other) { 
		desc = strdup(other.desc);
		return *this;
	}
	char* desc;
};

void foo(S* s) {
	printf("%s\n", s->desc);
}

int main() {
	S* s1 = new S(strdup("first"));
	S* s2 = new S(strdup("second"));
	S* s3 = s1;
	S s4 = *(s2);

	foo(s1);
	foo(s2);
	foo(s3);
	foo(&s4);
	
	delete s1->desc;
	delete s2->desc;

	return 0;
}
