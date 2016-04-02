#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sop/sop.h>

static int
ontexture(const sop_parser_state_t *state,
          const sop_parser_line_state_t line) {
  return SOP_EOK;
}

static int
oncomment(const sop_parser_state_t *state,
          const sop_parser_line_state_t line) {
  return SOP_EOK;
}

static int
onvertex(const sop_parser_state_t *state,
         const sop_parser_line_state_t line) {
  return SOP_EOK;
}

static int
onnormal(const sop_parser_state_t *state,
         const sop_parser_line_state_t line) {
  return SOP_EOK;
}

static int
onface(const sop_parser_state_t *state,
       const sop_parser_line_state_t line) {
  return SOP_EOK;
}

int
sop_parser_init(sop_parser_t *parser,
                sop_parser_options_t *options) {
  if (!parser) return SOP_EMEM;
  memset(parser, 0, sizeof(sop_parser_t));
#define SET_CALLBACK_IF(cb) \
  parser->callbacks. cb = options->callbacks. cb ? options->callbacks. cb : cb;
  SET_CALLBACK_IF(oncomment)
  SET_CALLBACK_IF(ontexture)
  SET_CALLBACK_IF(onvertex)
  SET_CALLBACK_IF(onnormal)
  SET_CALLBACK_IF(onface)
#undef SET_CALLBACK_IF
  parser->options = options;
  return SOP_EOK;
}


int
sop_parser_execute(sop_parser_t *parser,
                   const char *source,
                   size_t length) {
  // handle poor state and input
  if (!parser) {
    return SOP_EMEM;
  } else if (!source || 0 == length) {
    return SOP_EINVALID_SOURCE;
  }

  // sop state
  sop_parser_line_state_t line;
  sop_parser_state_t state;
  sop_enum_t type = SOP_NULL;

  // setup sate pointers
  state.line = &line;
  state.data = parser->options->data;

  // source state
  size_t bufsize = 0;
  char buffer[BUFSIZ];
  char prev = 0;
  char ch0 = 0;
  char ch1 = 0;
  int lineno = 0;
  int colno = 0;

  // init buffer
  memset(buffer, 0, BUFSIZ);

#define RESET_LINE_STATE {   \
  memset(buffer, 0, BUFSIZ); \
  lineno++;                  \
  bufsize = 0;               \
  colno = 0;                 \
}

  for (int i = 0; i < length; ++i) {
    ch0 = source[i];
    ch1 = source[i + 1];

    if (i > 0) {
      prev = source[i - 1];
    }

    if ((' ' == ch0 && '\n' == prev)) {
      RESET_LINE_STATE;
      continue;
    }

    if (' ' == ch0 && 0 == colno) {
      ch0 = ch1;
      ch1 = source[++i];
    }

#define CALL_CALLBACK_IF(cb, ...) {              \
  int rc = SOP_EOK;                              \
  if (parser->callbacks. cb) {                   \
    rc = parser->callbacks. cb(__VA_ARGS__);     \
    if (rc != SOP_EOK) return rc;                \
  }                                              \
}

    // we've reached the end of the line and now need
    // to notify the consumer with a callback, state error,
    // or continue if there is nothing to do
    if ('\n' == ch0) {
      if (!bufsize) {
        RESET_LINE_STATE;
        continue;
      }
      line.data = 0;
      line.type = type;
      line.length = bufsize;
      line.data = (void *) buffer;
      switch (type) {
        // continue until something meaningful
        case SOP_NULL: break;

        // handle comments
        case SOP_COMMENT: {
          line.data = (void *) buffer;
          CALL_CALLBACK_IF(oncomment, &state, line);
          break;
        }

        // handle directives
        case SOP_DIRECTIVE_VERTEX_TEXTURE: {
          float vertex[4];
          sscanf(buffer, "%f %f %f %f",
              &vertex[0], &vertex[1], &vertex[2], &vertex[3]);
          line.data = vertex;
          CALL_CALLBACK_IF(ontexture, &state, line);
          break;
        }

        case SOP_DIRECTIVE_VERTEX_NORMAL: {
          float vertex[4];
          sscanf(buffer, "%f %f %f %f",
              &vertex[0], &vertex[1], &vertex[2], &vertex[3]);
          line.data = vertex;
          CALL_CALLBACK_IF(onnormal, &state, line);
          break;
        }

        case SOP_DIRECTIVE_VERTEX: {
          float vertex[4];
          sscanf(buffer, "%f %f %f %f",
              &vertex[0], &vertex[1], &vertex[2], &vertex[3]);
          line.data = vertex;
          CALL_CALLBACK_IF(onvertex, &state, line);
          break;
        }

        case SOP_DIRECTIVE_FACE: {
          int maxfaces = 16;
          int faces[3][maxfaces];

          // vertex faces
          int *vf = faces[0];
          int x = 0;
          // vertex texture faces
          int *vtf = faces[1];
          int y = 0;
          // vertex normal faces
          int *vnf = faces[2];
          int z = 0;

          for (int i = 0; i < maxfaces; i++) {
            vf[i] = -1;
            vtf[i] = -1;
            vnf[i] = -1;
          }

          // current char buffer
          size_t size = 0;
          char buf[BUFSIZ];
          memset(buf, 0, BUFSIZ);

          // current face scope
          enum { READ = 0, VERTEX, TEXTURE, NORMAL };
          int scope = VERTEX;

          for (int j = 0; j < bufsize; ++j) {
            char c = buffer[j];

            // skip white space at beginning of line
            if (0 == size && ' ' == c) {
              continue;
            }

            if (j == bufsize - 1) {
              buf[size++] = c;
              goto read_face_data;
            }

            switch (c) {
              case '/':
              case '\t':
              case '\n':
              case ' ':
                goto read_face_data;

              default:
                buf[size++] = c;
                continue;
            }

read_face_data:
            switch (scope) {
              case VERTEX:
                sscanf(buf, "%d", &vf[x++]);
                break;

              case TEXTURE:
                sscanf(buf, "%d", &vtf[y++]);
                break;

              case NORMAL:
                sscanf(buf, "%d", &vnf[z++]);
                break;
            }

            switch (c) {
              case '/':
                scope++;
                break;

              case '\n':
              case '\t':
              case ' ':
                scope = VERTEX;
                break;
            }

            memset(buf, 0, BUFSIZ);
            size = 0;
          }

          line.data = faces;
          CALL_CALLBACK_IF(onface, &state, line)
          break;
        }

        // notify of memory errors
        case SOP_EMEM:
          return SOP_EMEM;

        // out of bounds if we get here for some reason
        default:
          return SOP_OOB;
      }

      RESET_LINE_STATE;
      continue;
    }

    if (0 == colno) {
      // v, vt, vn
      if ('v' == ch0) {
        if (' '== ch1) {
          type = SOP_DIRECTIVE_VERTEX;
          line.directive = "v";
        } else if ('t' == ch1) {
          type = SOP_DIRECTIVE_VERTEX_TEXTURE;
          (void) i++;
          line.directive = "vt";
        } else if ('n' == ch1) {
          type = SOP_DIRECTIVE_VERTEX_NORMAL;
          (void) i++;
          line.directive = "vn";
        } else {
          type = SOP_NULL;
        }
      } else if ('f' == ch0) {
        type = SOP_DIRECTIVE_FACE;
        line.directive = "f";
      } else if ('#' == ch0) {
        type = SOP_COMMENT;
        line.directive = "#";
      } else {
        switch (ch0) {
          case '\n':
            RESET_LINE_STATE;
            continue;
        }

        type = SOP_NULL;
      }
    } else {
      if (0 == bufsize && ' ' == ch0) {
        continue;
      }
      buffer[bufsize++] = ch0;
    }

    colno++;
  }
  return SOP_EOK;
#undef RESET_LINE_STATE
#undef CALL_CALLBACK_IF
}
