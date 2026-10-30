#ifndef STRING_H
#define STRING_H

struct String {
	char *buf;
	int   len;
};

inline bool strings_are_equal(String s1, String s2) {
	bool result = true;
	if (s1.len == s2.len) {
		for (int i = 0; i < s1.len; ++i) {
			result = result && (s1.buf[i] == s2.buf[i]);
		}
	} else {
		result = false;
	}

	return result;
}

inline String c_string(char *c_str) {
	String str = {};
	str.buf    = c_str;
	for (char *at = c_str; *at; ++at) {
		++str.len;
	}
	return str;
}

inline bool strings_are_equal(String s1, char *s2) {
	bool result = strings_are_equal(s1, c_string(s2));
	return result;
}

static bool is_digit(char c) {
	bool result = c >= '0' && c <= '9';
	return result;
}

inline int string_to_hex(String s) {
	int hex = 0;

	for (int i = 0; i < s.len; ++i) {
		char c = s.buf[i];

		if (is_digit(c)) {
			hex = (c - '0') + (hex * 10);
		} else if (c >= 'a' && c <= 'f')  {
			hex = ((c - 'a') + 10) + (hex * 10);
		} else if (c >= 'A' && c <= 'F') {
			hex = ((c - 'A') + 10) + (hex * 10);
		} else {
			break;
		}
	}

	return hex;
}

#endif
