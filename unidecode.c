
#include <stdio.h>
#include <stdlib.h>

#include <unicode/ustdio.h>

int main(int argc, char **argv) {
	char buf[256];
	UFILE *ui;
	UChar32 ch;
	UCharCategory chcat;
	UErrorCode ue = U_ZERO_ERROR;

	ui = u_finit(stdin, NULL, NULL);

	while ((ch = u_fgetcx(ui)) != U_EOF) {
		chcat = u_charType(ch);
		u_charName(ch, U_UNICODE_CHAR_NAME, buf, sizeof(buf), &ue);
		if(U_FAILURE(ue)) { snprintf(buf, sizeof(buf), "Error: %s", u_errorName(ue)); }
		if(ch <= 0xFFFF) {
			printf("U+%04x %s\n", (uint32_t)ch, buf);
		} else {
			printf("U+%08x %s\n", (uint32_t)ch, buf);
		}
	}

	u_fclose(ui);

	return 0;
}


