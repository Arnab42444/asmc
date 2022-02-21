
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#define TOK_CALL_OPEN ((char) 0x80)
#define TOK_CALL_CLOSED ((char) 0x81)
#define TOK_SHL ((char) 0x82)
#define TOK_SHR ((char) 0x83)
#define TOK_LE ((char) 0x84)
#define TOK_GE ((char) 0x85)
#define TOK_EQ ((char) 0x86)
#define TOK_INEQ ((char) 0x87)
#define TOK_AND ((char) 0x88)
#define TOK_OR ((char) 0x89)
#define TOK_DEREF ((char) 0x8a)
#define TOK_ADDR ((char) 0x8b)

int is_whitespace(char x) {
  return x == ' ' || x == '\t' || x == '\n';
}

void remove_spaces(char *begin, char *end) {
  char *read_buf = begin;
  while (read_buf != end) {
    if (*read_buf == '\0') {
      *begin = '\0';
      return;
    }
    if (is_whitespace(*read_buf)) {
      read_buf++;
    } else {
      *begin = *read_buf;
      begin++;
      read_buf++;
    }
  }
  if (begin != end) {
    *begin = '\0';
  }
}

int find_char(char *s, char *e, char c) {
  char *s2 = s;
  while (1) {
    if (s2 == e) {
      return -1;
    }
    if (*s2 == c) {
      return s2 - s;
    }
    if (*s2 == '\0') {
      return -1;
    }
    s2++;
  }
}

char *isstrpref(char *s1, char *s2) {
  while (1) {
    if (*s1 == '\0') {
      return s2;
    }
    if (*s1 != *s2) {
      return 0;
    }
    s1++;
    s2++;
  }
}

void trimstr(char *buf) {
  char *write_buf = buf;
  char *read_buf = buf;
  while (is_whitespace(*read_buf)) {
    read_buf++;
  }
  while (*read_buf != '\0') {
    *write_buf = *read_buf;
    write_buf++;
    read_buf++;
  }
  *write_buf = '\0';
  write_buf--;
  while (write_buf >= buf && is_whitespace(*write_buf)) {
    *write_buf = '\0';
    write_buf--;
  }
}

void fix_operands(char *begin, char *end) {
  while (begin + 1 < end) {
    if (*begin == '<' && *(begin+1) == '<') { *begin = TOK_SHL, *(begin+1) = ' '; }
    if (*begin == '>' && *(begin+1) == '>') { *begin = TOK_SHR, *(begin+1) = ' '; }
    if (*begin == '<' && *(begin+1) == '=') { *begin = TOK_LE, *(begin+1) = ' '; }
    if (*begin == '>' && *(begin+1) == '=') { *begin = TOK_GE, *(begin+1) = ' '; }
    if (*begin == '=' && *(begin+1) == '=') { *begin = TOK_EQ, *(begin+1) = ' '; }
    if (*begin == '!' && *(begin+1) == '=') { *begin = TOK_INEQ, *(begin+1) = ' '; }
    if (*begin == '&' && *(begin+1) == '&') { *begin = TOK_AND, *(begin+1) = ' '; }
    if (*begin == '|' && *(begin+1) == '|') { *begin = TOK_OR, *(begin+1) = ' '; }
    begin++;
  }
}

void print(char *begin, char *end) {
  while (begin < end) {
    if (*begin == '\0') {
      break;
    }
    putchar(*begin);
    begin++;
  }
}

void fix_strings(char *begin, char *end) {
  int mode = 0;
  while (begin < end) {
    assert((unsigned char) *begin < 0x80);
    if (mode == 0) {
      if (*begin == '\'') {
        mode = 1;
      } else if (*begin == '"') {
        mode = 2;
      } else if (begin + 1 < end && *begin == '/' && *(begin+1) == '/') {
        mode = 3;
        *begin = ' ';
        begin++;
        *begin = ' ';
      } else if (begin + 1 < end && *begin == '/' && *(begin+1) == '*') {
        mode = 4;
        *begin = ' ';
        begin++;
        *begin = ' ';
      } else if (*begin == '#') {
        mode = 3;
        *begin = ' ';
      }
    } else if (mode == 1) {
      if (*begin == '\'') {
        mode = 0;
      } else if (*begin == '\\') {
        assert(begin + 1 < end);
        *begin = *begin + 0x80;
        begin++;
        *begin = *begin + 0x80;
      } else {
        assert(*begin >= 0x20);
        *begin = *begin + 0x80;
      }
    } else if (mode == 2) {
      if (*begin == '"') {
        mode = 0;
      } else if (*begin == '\\') {
        assert(begin + 1 < end);
        *begin = *begin + 0x80;
        begin++;
        *begin = *begin + 0x80;
      } else {
        assert(*begin >= 0x20);
        *begin = *begin + 0x80;
      }
    } else if (mode == 3) {
      if (*begin == '\n') {
        mode = 0;
      } else {
        *begin = ' ';
      }
    } else if (mode == 4) {
      if (begin + 1 < end && *begin == '*' && *(begin+1) == '/') {
        mode = 0;
        *begin = ' ';
        begin++;
        *begin = ' ';
      } else {
        *begin = ' ';
      }
    }
    begin++;
  }
}

char *find_matching(char open, char closed, char *begin, char *end) {
  int depth = 0;
  while (begin < end) {
    if (*begin == open) {
      depth++;
    } else if (*begin == closed) {
      depth--;
    }
    if (depth == 0) {
      return begin;
    }
    begin++;
  }
  return 0;
}

void compile_statement(char *begin, char *end) {
  *end = '\0';
  trimstr(begin);
  if (*begin == '\0') {
    return;
  }
  char *s;
  if (s = isstrpref("unsigned char *", begin)) {
    remove_spaces(s, 0);
  } else if (s = isstrpref("unsigned char ", begin)) {
    remove_spaces(s, 0);
  } else if (s = isstrpref("unsigned int *", begin)) {
    remove_spaces(s, 0);
  } else if (s = isstrpref("unsigned int ", begin)) {
    remove_spaces(s, 0);
  } else if (s = isstrpref("char *", begin)) {
    remove_spaces(s, 0);
  } else if (s = isstrpref("char ", begin)) {
    remove_spaces(s, 0);
  } else if (s = isstrpref("int *", begin)) {
    remove_spaces(s, 0);
  } else if (s = isstrpref("int ", begin)) {
    remove_spaces(s, 0);
  } else if (s = isstrpref("void *", begin)) {
    remove_spaces(s, 0);
  } else {
    remove_spaces(begin, 0);
  }
  fprintf(stderr, "Statement: %s\n", begin);
}

void compile_block_with_head(char *def_begin, char *block_begin, char *block_end);

void compile_block(char *begin, char *end) {
  while (1) {
    int semicolon_pos = find_char(begin, end, ';');
    int brace_pos = find_char(begin, end, '{');
    if (semicolon_pos == -1 && brace_pos == -1) {
      *end = '\0';
      trimstr(begin);
      assert(*begin == '\0');
      break;
    } else {
      if (semicolon_pos == -1) {
        semicolon_pos = brace_pos + 1;
      }
      if (brace_pos == -1) {
        brace_pos = semicolon_pos + 1;
      }
      if (semicolon_pos < brace_pos) {
        compile_statement(begin, begin + semicolon_pos);
        begin = begin + semicolon_pos + 1;
      } else {
        char *res = find_matching('{', '}', begin+brace_pos, end);
        compile_block_with_head(begin, begin+brace_pos, res);
        begin = res + 1;
      }
    }
  }
}

void compile_block_with_head(char *def_begin, char *block_begin, char *block_end) {
  *block_begin = '\0';
  trimstr(def_begin);
  //remove_spaces(def_begin, 0);
  if (strcmp(def_begin, "enum") == 0) {
    return;
  }
  fprintf(stderr, "Begin of block: %s\n", def_begin);
  compile_block(block_begin+1, block_end-1);
  fprintf(stderr, "End of block\n");
}

int main() {
  int fd = open("cc.c", O_RDONLY);
  int len = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);
  char *src = mmap(0, len, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

  fix_strings(src, src+len);
  fix_operands(src, src+len);
  //remove_spaces(src, src+len);
  //print(src, src+len);
  compile_block(src, src+len);

  return 0;
}
