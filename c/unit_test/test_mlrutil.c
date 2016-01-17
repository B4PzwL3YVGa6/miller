#include <stdio.h>
#include <string.h>
#include "lib/minunit.h"
#include "lib/mlrutil.h"
#include "lib/mlrregex.h"

int tests_run         = 0;
int tests_failed      = 0;
int assertions_run    = 0;
int assertions_failed = 0;

// ----------------------------------------------------------------
static char * test_canonical_mod() {
	mu_assert("error: canonical_mod -7", mlr_canonical_mod(-7, 5) == 3);
	mu_assert("error: canonical_mod -6", mlr_canonical_mod(-6, 5) == 4);
	mu_assert("error: canonical_mod -5", mlr_canonical_mod(-5, 5) == 0);
	mu_assert("error: canonical_mod -4", mlr_canonical_mod(-4, 5) == 1);
	mu_assert("error: canonical_mod -3", mlr_canonical_mod(-3, 5) == 2);
	mu_assert("error: canonical_mod -2", mlr_canonical_mod(-2, 5) == 3);
	mu_assert("error: canonical_mod -1", mlr_canonical_mod(-1, 5) == 4);
	mu_assert("error: canonical_mod  0", mlr_canonical_mod(0, 5) == 0);
	mu_assert("error: canonical_mod  1", mlr_canonical_mod(1, 5) == 1);
	mu_assert("error: canonical_mod  2", mlr_canonical_mod(2, 5) == 2);
	mu_assert("error: canonical_mod  3", mlr_canonical_mod(3, 5) == 3);
	mu_assert("error: canonical_mod  4", mlr_canonical_mod(4, 5) == 4);
	mu_assert("error: canonical_mod  5", mlr_canonical_mod(5, 5) == 0);
	mu_assert("error: canonical_mod  6", mlr_canonical_mod(6, 5) == 1);
	mu_assert("error: canonical_mod  7", mlr_canonical_mod(7, 5) == 2);
	return 0;
}

// ----------------------------------------------------------------
static char * test_streq() {
	char* x;
	char* y;

	x = "";   y = "";   mu_assert_lf(streq(x, y) == !strcmp(x, y));
	x = "";   y = "1";  mu_assert_lf(streq(x, y) == !strcmp(x, y));
	x = "";   y = "12"; mu_assert_lf(streq(x, y) == !strcmp(x, y));

	x = "";   y = "";   mu_assert_lf(streq(x, y) == !strcmp(x, y));
	x = "a";  y = "";   mu_assert_lf(streq(x, y) == !strcmp(x, y));
	x = "ab"; y = "";   mu_assert_lf(streq(x, y) == !strcmp(x, y));

	x = "1";  y = "";   mu_assert_lf(streq(x, y) == !strcmp(x, y));
	x = "1";  y = "1";  mu_assert_lf(streq(x, y) == !strcmp(x, y));
	x = "1";  y = "12"; mu_assert_lf(streq(x, y) == !strcmp(x, y));

	x = "12"; y = "";   mu_assert_lf(streq(x, y) == !strcmp(x, y));
	x = "12"; y = "1";  mu_assert_lf(streq(x, y) == !strcmp(x, y));
	x = "12"; y = "12"; mu_assert_lf(streq(x, y) == !strcmp(x, y));

	x = "";   y = "a";  mu_assert_lf(streq(x, y) == !strcmp(x, y));
	x = "a";  y = "a";  mu_assert_lf(streq(x, y) == !strcmp(x, y));
	x = "ab"; y = "a";  mu_assert_lf(streq(x, y) == !strcmp(x, y));

	x = "";   y = "ab"; mu_assert_lf(streq(x, y) == !strcmp(x, y));
	x = "a";  y = "ab"; mu_assert_lf(streq(x, y) == !strcmp(x, y));
	x = "ab"; y = "ab"; mu_assert_lf(streq(x, y) == !strcmp(x, y));

	return 0;
}

// ----------------------------------------------------------------
static char * test_streqn() {
	char* x;
	char* y;

	x = "";    y = "";    mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));
	x = "";    y = "1";   mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));
	x = "";    y = "12";  mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));
	x = "";    y = "123"; mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));

	x = "";    y = "";    mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));
	x = "a";   y = "";    mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));
	x = "ab";  y = "";    mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));
	x = "abc"; y = "";    mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));

	x = "a";   y = "a";   mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));
	x = "a";   y = "aa";  mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));
	x = "a";   y = "ab";  mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));
	x = "a";   y = "abd"; mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));

	x = "ab";  y = "a";   mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));
	x = "ab";  y = "ab";  mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));
	x = "ab";  y = "abd"; mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));

	x = "abc"; y = "a";   mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));
	x = "abc"; y = "ab";  mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));
	x = "abc"; y = "abc"; mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));
	x = "abc"; y = "abd"; mu_assert_lf(streqn(x, y, 2) == !strncmp(x, y, 2));

	return 0;
}

// ----------------------------------------------------------------
static char * test_starts_or_ends_with() {

	mu_assert_lf(string_starts_with("abcde", ""));
	mu_assert_lf(string_starts_with("abcde", "a"));
	mu_assert_lf(string_starts_with("abcde", "abcd"));
	mu_assert_lf(string_starts_with("abcde", "abcde"));
	mu_assert_lf(!string_starts_with("abcde", "abcdef"));

	mu_assert_lf(string_ends_with("abcde", "", NULL));
	mu_assert_lf(string_ends_with("abcde", "e", NULL));
	mu_assert_lf(string_ends_with("abcde", "de", NULL));
	mu_assert_lf(string_ends_with("abcde", "abcde", NULL));
	mu_assert_lf(!string_ends_with("abcde", "0abcde", NULL));
	int len = -1;
	mu_assert_lf(!string_ends_with("abcde", "0abcde", &len));
	mu_assert_lf(len == 5);

	return 0;
}

// ----------------------------------------------------------------
static char * test_scanners() {
	mu_assert("error: mlr_alloc_string_from_double", streq(mlr_alloc_string_from_double(4.25, "%.4f"), "4.2500"));
	mu_assert("error: mlr_alloc_string_from_ull", streq(mlr_alloc_string_from_ull(12345LL), "12345"));
	mu_assert("error: mlr_alloc_string_from_int", streq(mlr_alloc_string_from_int(12345), "12345"));
	return 0;
}

// ----------------------------------------------------------------
static char * test_paste() {
	mu_assert("error: paste 2", streq(mlr_paste_2_strings("ab", "cd"), "abcd"));
	mu_assert("error: paste 3", streq(mlr_paste_3_strings("ab", "cd", "ef"), "abcdef"));
	mu_assert("error: paste 4", streq(mlr_paste_4_strings("ab", "cd", "ef", "gh"), "abcdefgh"));
	mu_assert("error: paste 5", streq(mlr_paste_5_strings("ab", "cd", "ef", "gh", "ij"), "abcdefghij"));
	return 0;
}

// ----------------------------------------------------------------
static char * test_unbackslash() {
	mu_assert_lf(streq(mlr_unbackslash(""), ""));
	mu_assert_lf(streq(mlr_unbackslash("hello"), "hello"));
	mu_assert_lf(streq(mlr_unbackslash("\\r\\n"), "\r\n"));
	mu_assert_lf(streq(mlr_unbackslash("\\t\\\\"), "\t\\"));
	mu_assert_lf(streq(mlr_unbackslash("[\\132]"), "[Z]"));
	mu_assert_lf(streq(mlr_unbackslash("[\\x59]"), "[Y]"));

	return 0;
}

// ----------------------------------------------------------------
static char * test_interpolate_regex_captures() {
	int was_allocated = FALSE;

	char* output = interpolate_regex_captures("hello", NULL, &was_allocated);
	mu_assert_lf(streq(output, "hello"));
	mu_assert_lf(was_allocated == FALSE);

	string_array_t* psa = string_array_alloc(0);
	output = interpolate_regex_captures("hello", psa, &was_allocated);
	mu_assert_lf(streq(output, "hello"));
	mu_assert_lf(was_allocated == FALSE);
	string_array_free(psa);

	// captures are indexed 1-up so the X is a placeholder at index 0
	psa = string_array_from_line(mlr_strdup_or_die("X,a,b,c"), ',');
	output = interpolate_regex_captures("hello", psa, &was_allocated);
	mu_assert_lf(streq(output, "hello"));
	mu_assert_lf(was_allocated == FALSE);
	string_array_free(psa);

	psa = string_array_from_line(mlr_strdup_or_die("X,a,b,c"), ',');
	output = interpolate_regex_captures("h\\3ello", psa, &was_allocated);
	printf("output=[%s]\n", output);
	mu_assert_lf(streq(output, "hcello"));
	mu_assert_lf(was_allocated == TRUE);
	string_array_free(psa);

	psa = string_array_from_line(mlr_strdup_or_die("X,a,b,c"), ',');
	output = interpolate_regex_captures("h\\1ello", psa, &was_allocated);
	printf("output=[%s]\n", output);
	mu_assert_lf(streq(output, "haello"));
	mu_assert_lf(was_allocated == TRUE);
	string_array_free(psa);

	psa = string_array_from_line(mlr_strdup_or_die("X,a,b,c"), ',');
	output = interpolate_regex_captures("h\\4ello", psa, &was_allocated);
	printf("output=[%s]\n", output);
	mu_assert_lf(streq(output, "h\\4ello"));
	mu_assert_lf(was_allocated == FALSE);
	string_array_free(psa);

	psa = string_array_from_line(mlr_strdup_or_die("X,a,b,c"), ',');
	output = interpolate_regex_captures("h\\0ello", psa, &was_allocated);
	printf("output=[%s]\n", output);
	mu_assert_lf(streq(output, "h\\0ello"));
	mu_assert_lf(was_allocated == FALSE);
	string_array_free(psa);

	psa = string_array_from_line(mlr_strdup_or_die("X,a,b,c"), ',');
	output = interpolate_regex_captures("h\\3e\\1l\\2l\\4o", psa, &was_allocated);
	printf("output=[%s]\n", output);
	mu_assert_lf(streq(output, "hcealbl\\4o"));
	mu_assert_lf(was_allocated == TRUE);
	string_array_free(psa);

	return 0;
}


// ================================================================
static char * all_tests() {
	mu_run_test(test_canonical_mod);
	mu_run_test(test_streq);
	mu_run_test(test_streqn);
	mu_run_test(test_starts_or_ends_with);
	mu_run_test(test_scanners);
	mu_run_test(test_paste);
	mu_run_test(test_unbackslash);
	mu_run_test(test_interpolate_regex_captures);
	return 0;
}

int main(int argc, char **argv) {
	printf("TEST_MLRUTIL ENTER\n");
	char *result = all_tests();
	printf("\n");
	if (result != 0) {
		printf("Not all unit tests passed\n");
	}
	else {
		printf("TEST_MLRUTIL: ALL UNIT TESTS PASSED\n");
	}
	printf("Tests      passed: %d of %d\n", tests_run - tests_failed, tests_run);
	printf("Assertions passed: %d of %d\n", assertions_run - assertions_failed, assertions_run);

	return result != 0;
}
