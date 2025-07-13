#include <langinfo.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

int
main()
{
	char buffer[1024];
	locale_t fr_locale_t;

	fr_locale_t = newlocale(LC_ALL_MASK, "fr_FR.UTF-8", 0);
	if (fr_locale_t == 0)
		return EXIT_FAILURE;

	printf("=== with global locale set to C ===\n");
	setlocale(LC_ALL, "C");
	printf("%s\n", nl_langinfo_l(DAY_1, LC_GLOBAL_LOCALE));
	printf("%s\n", nl_langinfo_l(DAY_1, fr_locale_t));
	printf("%s\n", nl_langinfo_l(DAY_1, NULL));
	printf("=== with global locale set to French ===\n");
	setlocale(LC_ALL, "fr_FR.UTF-8");
	printf("%s\n", nl_langinfo_l(DAY_1, LC_GLOBAL_LOCALE));
	printf("%s\n", nl_langinfo_l(DAY_1, fr_locale_t));
	printf("%s\n", nl_langinfo_l(DAY_1, NULL));
}
