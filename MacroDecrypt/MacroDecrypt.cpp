#include <stdio.h>
#include <string.h>
#include <malloc.h>

void my_print(int indent, const char *ptr, int length)
{
  for (int i=0; i<indent*2; i++)
    printf(" ");
  for (int i=0; i<length; i++, ptr++)
    printf("%c",*ptr);
}

void help_print()
{
   printf("  MacroDecrypt 2.3    addition for FAR Manager\n");
   printf("  (c) Alex Yaroslavsky, (c) Alexey Samlyukov\n");
   printf("\nUsege: MacroDecrypt <command> <macro reg-file>\n");
   printf("\n<command>:\n");
   printf("  -d: Decrypt macro to strings\n");
   printf("  -e: Encrypt macro from strings to one string (REG_SZ)\n");
   printf("  -m: Convert \"Sequence\" from string(s) to REG_MULTI_SZ\n");
   printf("\nExample:\n");
   printf("  MacroDecrypt -d macro_file.reg\n");
}

int main(int argc, char **argv)
{
  int options=-1;
  while (--argc>0 && (*++argv)[0]=='-')
    switch(*++argv[0])
    {
      case 'd':            // decrypt macro
        options=0; break;
      case 'e':            // encrypt macro to REG_SZ
        options=1; break;
      case 'm':            // convert "Sequence" to REG_MULTI_SZ
        options=2; break;
      default:             // error
        break;
    }

  if (argc<1 || options<0) { help_print(); return 1; }

  FILE *f;
  if (!(f=fopen(argv[0],"r"))) return 1;

  fseek(f,0,SEEK_END);
  int size = ftell(f);
  fseek(f,0,SEEK_SET);

  char *buf = (char *) malloc(size+1);
  size = fread(buf,1,size,f);
  buf[size] = 0;
  fclose(f);

  char *ptr = buf;
  int line = 0;

  switch (options)
  {

///----------------------------------------------------------------------------
    case 0:
    {
      int indent = 0;
      int normal_out = 0;
      int inquotes = 0;
      int space = 0;
      int space_line = line;
      int i=0;
      int intmpbuf=0;
      char *saveptr=0, *endptr=0;
      char *tmpbuf = (char *) malloc(size);

      while (*ptr)
      {
        if (!_strnicmp(ptr,"\"Sequence\"=hex(7):",18))
        {
          i=0;
          printf("\"Sequence\"=");
          ptr+=18;

          if (*ptr == ' ' || *ptr == '\n' || *ptr == '\r')
            continue;

          intmpbuf=1;
          char c_hex[3]={'\0'};
          int c;

          while (*ptr && *ptr != '"')
          {
            sscanf(strncpy(c_hex,ptr,2), "%x", &c);
            tmpbuf[i]=(c?c:' ');
            i++;
            ptr+=3;

            while (*ptr == ' ' || *ptr == '\\' || *ptr == '\n' || *ptr == '\r')
              ptr++;
          }
          endptr=ptr-1;
        }
        if (intmpbuf)
        {
          ptr=&tmpbuf[0];
          intmpbuf=0;
        }
        if (i && ptr==&tmpbuf[i-1])
          ptr=endptr;

        if (*ptr==' ' && (!_strnicmp(ptr," $if",4) || !_strnicmp(ptr," $rep",5)
                      || !_strnicmp(ptr," $while",7) || !_strnicmp(ptr," $else",6)
                      || !_strnicmp(ptr," $end",5)))
          ptr++;

        if (*ptr == '$')
        {
          saveptr = ptr;
          ptr++;
          if (!_strnicmp(ptr,"if",2) || !_strnicmp(ptr,"rep",3) || !_strnicmp(ptr,"while",5))
          {
            while (*ptr && *ptr != '(') ptr++;
            if (*ptr)
            {
              ptr++;
              int bracket=1;
              while (bracket && *ptr)
              {
                if (*ptr == '(')
                  bracket++;
                if (*ptr == ')')
                  bracket--;
                ptr++;
              }
            }
            if (line)
              printf("\n");
            line++;
            my_print(indent,saveptr,ptr-saveptr);
            indent++;
            while (*ptr == ' ')
              ptr++;
            normal_out = 0;
            continue;
          }
          else if (!_strnicmp(ptr,"else",4))
          {
            indent--;
            ptr += 4;
            if (line)
              printf("\n");
            line++;
            my_print(indent,saveptr,ptr-saveptr);
            indent++;
            while (*ptr == ' ')
              ptr++;
            normal_out = 0;
            continue;
          }
          else if (!_strnicmp(ptr,"end",3))
          {
            indent--;
            ptr += 3;
            if (line)
              printf("\n");
            line++;
            my_print(indent,saveptr,ptr-saveptr);
            while (*ptr == ' ')
              ptr++;
            normal_out = 0;
            continue;
          }
          ptr = saveptr;
        }

        if (!normal_out)
        {
          if (line)
            printf("\n");
          line++;
          my_print(indent,"",0);
          normal_out = 1;
        }

        if (*ptr == '"')
        {
          if (inquotes && *(ptr-1) != '\\' && (ptr - buf > 2) && *(ptr-2) != '\\' && *(ptr-3) != '\\')
            inquotes = 0;
          else
            inquotes = 1;
        }
        if (*ptr == ' ' && !inquotes)
        {
          if (!space)
            space_line = line;
          space = 1;
        }
        else
        {
          if (space && space_line == line)
            printf(" ");
          space = 0;
          printf("%c",*ptr);
        }
        ptr++;
      }

      free(buf);
      free(tmpbuf);
      return 0;
    }

///----------------------------------------------------------------------------
    case 1:
    {
      while (*ptr)
      {
        if (*ptr == '"' && !_strnicmp(ptr,"\"=\"",3))
        {
          char *startquotes=ptr+2;
          while (*ptr)
          {
            while (*ptr == ' ')
              ptr++;

            while (*ptr && *ptr != '\n' && *ptr != '\r')
            {
              printf("%c",*ptr);
              ptr++;

              if (*ptr && *(ptr-1) == '"' && ptr-1 > startquotes && *(ptr-2) != '\\' &&
                   (ptr-1-buf > 2) && *(ptr-3) != '\\' && *(ptr-4) != '\\' )
                // ну вот и дошли до конца нашего блока...
                goto NORMAL_OUT;
            }

            while (*ptr == '\n' || *ptr == '\r')
            {
              ptr++; line++;
            }

            if (*ptr && line) printf(" ");
          }
        }
        else
        {
   NORMAL_OUT:
          printf("%c",*ptr);
          line=0;
          ptr++;
        }
      }
      free(buf);
      return 0;
    }

///----------------------------------------------------------------------------
//  TODO: делить hex(7) на 20 символов - 1-я, 25 символов - 2-я строка и последующие...
    case 2:
    {
      while (*ptr)
      {
        if (*ptr == '"' && !_strnicmp(ptr,"\"Sequence\"=\"",12))
        {
          char *startquotes=ptr+11;
          while (*ptr)
          {
            while (*ptr == '\n' || *ptr == '\r')
              ptr++;
            while (*ptr && *ptr != '\n' && *ptr != '\r')
            {
              if (ptr == startquotes)
                printf("hex(7):");

              if (ptr<startquotes)
                printf("%c",*ptr);
              else
                printf("%x,",*ptr);
              ptr++;

              // дошли до конца Sequence...
              if (*ptr && *(ptr-1) == '"' && ptr-1 > startquotes && *(ptr-2) != '\\' &&
                   (ptr-1-buf > 2) && *(ptr-3) != '\\' && *(ptr-4) != '\\' )
              {
                printf("00");
                goto NORMAL_OUT2;
              }
            }

            if (*ptr && line) printf("00,");
            line++;
          }
        }
        else
        {
   NORMAL_OUT2:
          printf("%c",*ptr);
          line=0;
          ptr++;
        }
      }
      free(buf);
      return 0;
    }
  }

  return 0;
}
