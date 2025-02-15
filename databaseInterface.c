#include <stdio.h>
#include <stdlib.h>
#include <string.h> 

#define INIT_SIZE 256

// Single Structure Datatype 80 bytes
typedef struct Entry {
  int ssn; 
  char first[32];
  char last[32];
  char YYT[4];
  int p1, p2;     // Performance counters 
} Entry;

// 5 byte key struct to hold the ssh associated with a key
typedef struct Key {
  int ssn;
  char val;
} Key;

// Structure of struct entry array 32 bytes
typedef struct Table {
  Entry *list;
  Key *keys;
  long size;
  long max;
} Table;

// encrypt and decrypt function pointer type 
typedef void (*secure)(char, char*);

/* Inlined Simple Functions */

// Recrypts a value based on its key 
static inline void encrypt(char key, char str[]) {
  int len = strlen(str), i, j, shift, wrap = 0;

  for (i = 0; i < len; i++) {
    if (str[i] == '_') continue;
    str[i] += key;
    str[i] -= 'z';
    str[i] = 'z' + str[i] % 26;
  }
}

// Decrypts a value based on its key
static inline void decrypt(char key, char str[]) {
  int len = strlen(str), i, j, shift, wrap = 0;

  // same structure and encrypt just wraps the other direction
  // sets 'base' char at 'z' instead of 'a' and decrements
  //
  //
  for (i = 0; i < len; i++) {
    if (str[i] == '_') continue;
    str[i] -= key;
    str[i] -= 'a';
    str[i] = 'z' + str[i] % 26;
  }

  /*
  shift = key - 'g';
  for (i = 0; i < len; i++) {
    if (str[i] == '_') continue;
    for (j = 0; j < shift; j++) {
      if (str[i] == 'a') {
        str[i] = 'z';
      } else
        str[i]--;   // Decrement if it didn't wrap
    }
  }*/
}

// Forces all chars to lower case
static inline void normalize(char str[]) {
  int i = 0;
  // If char is uppercase transform to lower 
  while (str[i] != 0) {
    if (str[i] >= 'A' && str[i] <= 'Z') {
      str[i] += 'a' - 'A';
    }
    i++;
  }
}

// Checks if all character values are alphabetical chars
static inline int charValid(char str[]) {
  int i = 0;
  while (str[i] != 0) {
    // Checks if string is within lowercase range
    if (str[i] >= 'a' && str[i] <= 'z') {
      i++;
      continue;
    }

    // Checks if in uppercase range 
    if (str[i] >= 'A' && str[i] <= 'Z') {
      i++;
      continue;
    }
    // If neither were try then return false
    return 0;
  }
  return 1;
}

// Checks if all chars are numeric
static inline int intValid(char str[]) {
  int i = 0;
  while (str[i] != 0) {
    // Checks if char is numeric
    if (str[i] >= '0' && str[i++] <= '9') continue;
    
    return 0;
  }
  return 1;
}

// intValid already filters negative inputs 
// Convert string to integer value 
static inline int strToInt(char str[]) {
  int i = 0, result = 0;
  while (str[i] != 0) {
    result *= 10;
    result += str[i++] - '0';
  }
  return result;
}

// Function definitionns 
void secureData(int start, int stop, Table *table, secure func);
int execute(Table *table, char cmd[], int iter);
void entrySearch(Table *table, int *saved);
void delete(Table *table, int index, int *saved);
void modify(Table *table, int index, int *saved);
void resize(Table *table, int *saved);
void addEntry(Table *table, int *saved);
void displayTable(const Table *table);
void sortTable(Table *table, int *saved);
void saveTable(Table *table, int *saved);     // Encrypt and print to specified file. 
void loadData(Table *table, int *saved);
int closeTable(Table *table, int saved);      // Free resources associated 

int main(int argc, char *argv[]) {
  FILE *recruit_ptr = NULL, *secure_ptr = NULL;
  char buffer[64];
  Table table;

  int running = 1, entry_count = 0, scan = 0, iter = 0;
  // counters 
  int entry = 0, key = 0, res = 0;

  // Initialize data contianer
  table.max  = INIT_SIZE;
  table.size = 0;
  table.list = (Entry *)malloc(table.max * sizeof(Entry));
  table.keys = (Key *)calloc(table.max, sizeof(Key));

  if (argc != 1 && argc != 3) {
    printf("Invalid call to main. Usage: [./exec] [recruit.txt] [codes.txt]\n");
    free(table.list);
    free(table.keys);
    return 1;
  }

  if (argc == 1) {
    printf("Enter a data file >> ");
    scanf("%s", buffer);
    recruit_ptr = fopen(buffer, "r");
  } else {
    recruit_ptr = fopen(argv[1], "r");
  }
  
  if (recruit_ptr == NULL) {
    printf("Recruit >> File error\n");
    free(table.list);
    free(table.keys);
    return 1;
  }

  if (argc == 1) {
    printf("Enter a decryption file >> ");
    scanf("%s", buffer);
    secure_ptr = fopen(buffer, "r");
  } else {
    secure_ptr = fopen(argv[2], "r");
  }

  // Check for valid file
  if (secure_ptr == NULL) {
    printf("Decryption >> File error\n");
    free(table.list);
    free(table.keys);
    return 1;
  }
  
  // Read files specified by user into list array 
  while (running == 1) {
    if (table.size == table.max) {
      // Resize arrays to 2x(curr_siz) (Both arrays)
      resize(&table, (void *)0);
    } 

    scan = fscanf(recruit_ptr, "%d", &table.list[entry].ssn);
    if (scan == EOF) {
      running = 0;
    } else {
      fscanf(recruit_ptr, "%s", table.list[entry].last);
      fscanf(recruit_ptr, "%s", table.list[entry].first);
      fscanf(recruit_ptr, "%s", table.list[entry].YYT);
      fscanf(recruit_ptr, "%d", &table.list[entry].p1);
      fscanf(recruit_ptr, "%d", &table.list[entry].p2);

      entry++;
      table.size++;
    }
  }

  // Read from security file into keys array 
  for (key = 0; key < table.size; key++) {
    // Scan key into array then scan value after into garbage buffer 
    fscanf(secure_ptr, "%d", &table.keys[key].ssn); 
    fscanf(secure_ptr, " %c", &table.keys[key].val);
  }
  // Decrypt Data
  secureData(0, table.size, &table, decrypt);

  // Table loop 
  fclose(recruit_ptr);
  fclose(secure_ptr);

  running = 1;
  iter = 0;
  while (running == 1) {
    
    printf("\n>> Enter a Valid Command; Usage: [Keyword]:\n");
    printf(">  [Display] data\n");
    printf(">  [Sort] data\n");
    printf(">  [Search] Entry by Name\n");
    printf(">  [Add] New Entry\n");
    printf(">  [Save] to File\n");
    printf(">  [Load] New Files\n");
    printf(">  [Close] Program\n");
    printf(">> ");
    scanf("%s", buffer);
    res = execute(&table, buffer, iter);
    if (res == -1) {
      running = 0;
    } else if (res == 1) {
      printf("\n>> Invalid Command!\n");
    }

    iter++;
  } // Data freed by execution of close command

  return 0;
}

void secureData(int start, int stop, Table *table, secure func) {
  int i, j;

  // Iterate from start and end index of input pair
  for (i = start; i < stop; i++) {
    // Iterate over all keys
    for (j = 0; j < table->size; j++) {
      
      // Check ssn in keys against list arrays and use secure func with key and value pair
      if (table->keys[j].ssn == table->list[i].ssn) {
        func(table->keys[j].val, table->list[i].last);
        func(table->keys[j].val, table->list[i].first);
      }
    }
  }
}

// Execute submitted command
int execute(Table *table, char cmd[], int i) {
  static int saved;
  if (i == 0) saved = 1;

  // Check input as valid
  if (!charValid(cmd)) return 1; 

  // Shift to lowercase
  normalize(cmd);

  printf(">> Command %s selected!\n\n", cmd);

  // Execute matching command. 
  // If none match return 1 to denote an invalid command was recieved
  if (strcmp(cmd, "display") == 0) {
    displayTable(table);
  } else if (strcmp(cmd, "sort") == 0) {
    sortTable(table, &saved);
  } else if (strcmp(cmd, "search") == 0) {
    entrySearch(table, &saved);
  } else if (strcmp(cmd, "add") == 0) {
    if (table->size + 1 > table->max) resize(table, &saved);
    addEntry(table, &saved);
  } else if (strcmp(cmd, "save") == 0) {
    saveTable(table, &saved);
  } else if (strcmp(cmd, "load") == 0) {
    loadData(table, &saved);
  } else if (strcmp(cmd, "close") == 0) {
    return closeTable(table, saved); 
  } else {
    return 1;
  }

  return 0;         // Successful execution 
}

// Adds a new entry by prompting the user with new inputs
void addEntry(Table *table, int *saved) {
  // Enter new values for entry
  printf("\nEnter the New Entry >>");
  scanf("%d", &table->list[table->size].ssn);
  scanf("%s", table->list[table->size].last);
  scanf("%s", table->list[table->size].first);
  scanf("%s", table->list[table->size].YYT);
  scanf("%d", &table->list[table->size].p1);
  scanf("%d", &table->list[table->size].p2);

  // Ensure new entry has an encrpytion key 
  printf("\nEnter a encryption key >> ");
  scanf(" %c", &table->keys[table->size].val);
  table->keys[table->size].ssn = table->list[table->size].ssn;

  table->size++;
  *saved = 0;
}

// Resize array to 2x previous size 
void resize(Table *table, int *saved) {
  Table tmp;
  int i;
  table->max *= 2;
  
  // Allocate 2x space
  tmp.list = (Entry *)malloc(table->max * sizeof(Entry));
  tmp.keys = (Key *)calloc(table->max, sizeof(Key));

  // tmp = a 
  for (i = 0; i < table->size; i++) {
    tmp.list[i] = table->list[i];
    tmp.keys[i] = table->keys[i];
  }

  // Free original memory -> a = b
  free(table->list);
  free(table->keys);

  // Take ownership of ptr to memory -> b = a 
  table->list = tmp.list;
  table->keys = tmp.keys;
  *saved = 0;
}

// Print Entry and Prompt for new values to be scanned. Doesn't check for new valid entry values
void modify(Table *table, int index, int *saved) {
  char buffer[64];
  int value = 0;

  // Display entry to user as a reference 
  printf("Entry %d: [%09d]: %s, %s : %3s [%6d]  [%6d]\n",
    index,
    table->list[index].ssn,
    table->list[index].last,
    table->list[index].first,
    table->list[index].YYT,
    table->list[index].p1,
    table->list[index].p2
  );

  // Scan new entry in 
  printf("\nEnter the Modified Entry >>");
  scanf("%d", &table->list[index].ssn);
  scanf("%s", table->list[index].last);
  scanf("%s", table->list[index].first);
  scanf("%s", table->list[index].YYT);
  scanf("%d", &table->list[index].p1);
  scanf("%d", &table->list[index].p2);
  *saved = 0;
}

// Fill new array with all values but deleted index
void delete(Table *table, int index, int *saved) {
  int i, new = 0;
  Table tmp;
  // Create memory for temp list
  tmp.list = (Entry *)malloc((table->size - 1) * sizeof(Entry));
  tmp.keys = (Key *)malloc((table->size - 1) * sizeof(Key));

  // Iterate over list. If list index matches than don't copy it else copy 
  for (i = 0; i < table->size; i++) {
    if (i == index) continue;
    tmp.list[new] = table->list[i];
    tmp.keys[new] = table->keys[i];
    new++;
  }

  // Adjust table parameters
  table->size--;
  free(table->list);
  free(table->keys);

  // Transfer ownership of list and keys ptrs to master table 
  table->list = tmp.list;
  table->keys = tmp.keys;
  *saved = 0;
}

// Return array of indices with matching value 
static int *ssnSearch(Table *table, int value, int *ct) {
  int *results, entry, count = 0, tmp = 0, i = 0; 

  // Get match count 
  for (entry = 0; entry < table->size; entry++) 
    if (table->list[entry].ssn == value) count++; 

  // Return NULL for an empty list 
  if (count == 0)
    return NULL;

  // alloc 
  results = (int *)malloc(count * sizeof(int));

  // Add index to output if matches
  for (entry = 0; entry < table->size; entry++) {
    if (tmp == count) break;
    if (table->list[entry].ssn == value) {
      results[i++] = entry;
    }
  }

  *ct = count; 
  // return filled array
  return results; 
}

static int *lastSearch(Table *table, char bufr[], int *ct) {
  int *results, entry, count = 0, tmp = 0, i = 0; 

  // Get match count 
  for (entry = 0; entry < table->size; entry++) 
    if (strcmp(bufr, table->list[entry].last) == 0) count++; 

  if (count == 0)
    return NULL;

  // alloc 
  results = (int *)malloc(count * sizeof(int));

  // Add index to output if matches
  for (entry = 0; entry < table->size; entry++) {
    if (tmp == count) break;
    if (strcmp(bufr, table->list[entry].last) == 0) {
      results[i++] = entry;
    }
  }

  *ct = count; 
  // return filled array
  return results; 
}

static int *firstSearch(Table *table, char bufr[], int *ct) {
  int *results, entry, count = 0, tmp = 0, i = 0; 

  // Get match count 
  for (entry = 0; entry < table->size; entry++) 
    if (strcmp(bufr, table->list[entry].first) == 0) count++; 

  if (count == 0)
    return NULL;

  // alloc 
  results = (int *)malloc(count * sizeof(int));

  // Add index to output if matches
  for (entry = 0; entry < table->size; entry++) {
    if (tmp == count) break;
    if (strcmp(bufr, table->list[entry].first) == 0) {
      results[i++] = entry;
    }
  }

  *ct = count; 
  // return filled array
  return results; 
}

static int *termSearch(Table *table, char bufr[], int *ct) {
  int *results, entry, count = 0, tmp = 0, i = 0; 

  // Get match count 
  for (entry = 0; entry < table->size; entry++) 
    if (strcmp(bufr, table->list[entry].YYT) == 0) count++; 

  if (count == 0)
    return NULL;

  // alloc 
  results = (int *)malloc(count * sizeof(int));

  // Add index to output if matches
  for (entry = 0; entry < table->size; entry++) {
    if (tmp == count) break;
    if (strcmp(bufr, table->list[entry].YYT) == 0) {
      results[i++] = entry;
    }
  }

  *ct = count; 
  // return filled array
  return results; 
}

static int *performanceSearch(Table *table, int value, int *ct) {
  int *results, entry, count = 0, tmp = 0, i = 0; 

  // Get match count 
  for (entry = 0; entry < table->size; entry++) 
    if (table->list[entry].p1 == value || table->list[entry].p2 == value) count++; 

  // Return null for no matches 
  if (count == 0)
    return NULL;

  // alloc 
  results = (int *)malloc(count * sizeof(int));

  // Add index to output if matches
  for (entry = 0; entry < table->size; entry++) {
    if (tmp == count) break;
    if (table->list[entry].p1 == value || table->list[entry].p2 == value) {
      results[i++] = entry;
    }
  }

  *ct = count; 
  // return filled array
  return results; 
}

void entrySearch(Table *table, int *saved) {
  char field_bufr[16], bufr[16];
  int exec = 0, value = 0;
  int *matches = NULL, ct = 0, i;
  char root[3];

  do {
    printf("\n>> Enter in Valid Search Format: [field value] (q to quit)\n");
    printf(">  [SSN %%9d]\n");
    printf(">  [First %%s]\n");
    printf(">  [Last %%s]\n");
    printf(">  [Term %%3s]\n");
    printf(">  [Performance %%3d]\n");    // Will check both performance fields
    printf(">> ");
    scanf("%s", field_bufr);

    // Test for quit input
    if (field_bufr[0] == 'q') break;

    if (!charValid(field_bufr)) continue;

    normalize(field_bufr);
    scanf("%s", bufr);

    // Handle term input (mix of char and int)
    if (strcmp(field_bufr, "term") == 0) {                    // YYT section
      if ((bufr[2] >= 'A' && bufr[2] <= 'Z') != 1) continue;  // Check char section of T
      
      // Copy potential ints from buffer
      root[0] = bufr[0];
      root[1] = bufr[1];

      if (!intValid(root)) continue;                          // Check int section of YY

      matches = termSearch(table, bufr, &ct);
      exec = 1;
    }

    // Handle integer input
    if (strcmp(field_bufr, "first") != 0 && strcmp(field_bufr, "last") != 0) {
      if (!intValid(bufr)) continue;
      value = strToInt(bufr);     // Convert to integer 

      if (strcmp(field_bufr, "ssn") == 0) {
        matches = ssnSearch(table, value, &ct);  
        exec = 1;
      } else if (strcmp(field_bufr, "performance") == 0) {
        matches = performanceSearch(table, value, &ct);
        exec = 1;
      }
    }

    if (!charValid(bufr)) continue;

    if (strcmp(field_bufr, "last") == 0) {
      matches = lastSearch(table, bufr, &ct);
      exec = 1;
    }
    
    if (strcmp(field_bufr, "first") == 0) {
      matches = firstSearch(table, bufr, &ct);
      exec = 1;
    }
    // Breaks if a valid search was executed else reprints options table
  } while (exec != 1);

  if (matches == NULL) {
    printf("\nNo matches to edit\n");
    return;
  }
  
  // Ask to modify or delete searched entry
  for (i = 0; i < ct; i++) {
    exec = 0;
    do {
      printf("\n>> Entry %d\n", matches[i]);
      printf(">  [Modify]\n");
      printf(">  [Delete]\n");
      printf(">  [Skip]\n");
      printf(">> ");
      scanf("%s", bufr);
      if (!charValid(bufr)) continue;
      normalize(bufr);

      if (strcmp(bufr, "skip") == 0) {
        exec = 1;
        continue;
      };
      
      if (strcmp(bufr, "delete") == 0) {
        delete(table, matches[i], saved);
        exec = 1;
        continue;
      }
      
      if (strcmp(bufr, "modify") == 0) {
        modify(table, matches[i], saved);
        exec = 1;
        continue;
      }
    } while (exec != 1);  
  } 
  
  // Free match array
  free(matches);
}

void displayTable(const Table *table) {
  int i;

  // heading 
  printf("/------------------------------------------------------------------------\\\n");
  printf("|    SSN    |      LAST        |     FIRST        | YYT |   PERFORMANCE  |\n");
  printf("|-----------+------------------+------------------+-----+----------------|\n");

  for (i = 0; i < table->size; i++) {
    printf("| %09d | %16s | %16s | %3s | %5d  %5d   |\n",
      table->list[i].ssn,
      table->list[i].last,
      table->list[i].first,
      table->list[i].YYT,
      table->list[i].p1,
      table->list[i].p2
    );
  }

  // bottom heading 
  printf("\\------------------------------------------------------------------------/\n");
}


// Simple swap of two values
static void swapT(Table *table, int index) {
  Entry tmp; 
  tmp                    = table->list[index];
  table->list[index]     = table->list[index + 1];
  table->list[index + 1] = tmp;
}

static void swapV(int *values, int index) {
  values[index] ^= values[index + 1];
  values[index + 1] ^= values[index];
  values[index] ^= values[index + 1];
}

static void swapC(char **values, int index) {
  char buffer[32];
  strcpy(buffer, values[index]);
  strcpy(values[index], values[index + 1]);
  strcpy(values[index + 1], buffer);
}

// All sorting algorithms are naive bubble sorts 

void termSort(Table *table, char *values[], char type[16]) {
  int i, sort = 0;
  int lookup[89];
  char year1[4], year2[4];
  int tmp1, tmp2;

  // Set lookup
  lookup[70] = 4;
  lookup[88] = 3;
  lookup[83] = 2;
  lookup[87] = 1;
  // extract the integer value from YY and then compare char in T to lookup

  do {
    sort = 1;
    if (strcmp(type, "asc") == 0) {
      for (i = 0; i < table->size - 1; i++) {
        strcpy(year1, values[i]);
        strcpy(year2, values[i + 1]);
        year1[2] = year2[2] = 0;  // Null terminate before char value T 
        tmp1 = strToInt(year1);
        tmp2 = strToInt(year2);

        // If same year compare by term 
        if (tmp1 == tmp2) {
          // If ith date is after i + 1th then it is "greater"
          if (lookup[values[i][2]] > lookup[values[i + 1][2]]) {
            swapT(table, i);
            swapC(values, i);
            sort = 0;
          }
        }

        // Swap if years are explicitly differernt 
        if (tmp1 > tmp2) {
          swapT(table, i);
          swapC(values, i);
          sort = 0;
        }
      }
    } else if (strcmp(type, "des") == 0) {
      for (i = 0; i < table->size - 1; i++) {
        strcpy(year1, values[i]);
        strcpy(year2, values[i + 1]);
        year1[2] = year2[2] = 0;  // Null terminate before char value T 
        tmp1 = strToInt(year1);
        tmp2 = strToInt(year2);

        // If same year compare by term 
        if (tmp1 == tmp2) {
          // If ith date is after i + 1th then it is "greater"
          if (lookup[values[i][2]] < lookup[values[i + 1][2]]) {
            swapT(table, i);
            swapC(values, i);
            sort = 0;
          }
        }

        // Swap if years are explicitly differernt 
        if (tmp1 < tmp2) {
          swapT(table, i);
          swapC(values, i);
          sort = 0;
        }
      }
    } // Returns unsorted if type was invalid 
  } while (sort == 0);
}

void intSort(Table *table, int *values, char type[16]) {
  int sort = 0, i;
  do {
    sort = 1;
    if (strcmp(type, "asc") == 0) {
      for (i = 0; i < table->size - 1; i++) {
        if (values[i] > values[i + 1]) {
          swapT(table, i);
          swapV(values, i);
          sort = 0;
        }
      }
    } else if (strcmp(type, "des") == 0) {
      for (i = 0; i < table->size - 1; i++) {
        if (values[i] < values[i + 1]) {
          swapT(table, i);
          swapV(values, i);
          sort = 0;
        }
      }
    }
  } while (sort == 0);
}

void stringSort(Table *table, char **values, char type[16]) {
  int sort = 0, i;
  do {
    sort = 1;
    if (strcmp(type, "asc") == 0) {
      for (i = 0; i < table->size - 1; i++) {
        if (strcmp(values[i], values[i + 1]) > 0) {
          swapT(table, i);
          swapC(values, i);
          sort = 0;
        }
      }
    } else if (strcmp(type, "des") == 0) {
      for (i = 0; i < table->size - 1; i++) {
        if (strcmp(values[i], values[i + 1]) < 0) {
          swapT(table, i);
          swapC(values, i);
          sort = 0;
        }
      }
    }
  } while (sort == 0);
}

void sortTable(Table *table, int *saved) {
  char buffer[64], type[16];
  int exec = 0, i, int_sort = 0, term_sort = 0, string_sort = 0;
  // Parallel arrays to load as comparators
  int *arrayI   = (int *)malloc(table->size * sizeof(int));
  char **arrayS = (char **)malloc(table->size * sizeof(char*));
  for (i = 0; i < table->size; i++) {
    arrayS[i] = (char *)calloc(32, sizeof(char));
  }

  do {
    // Prompt sort field and ascending or descending 
    printf("\n>> Enter in Valid Sort Format: [field asc/des] (q to quit)\n");
    printf(">  [SSN asc/des]\n");
    printf(">  [First asc/des]\n");
    printf(">  [Last asc/des]\n");
    printf(">  [Term asc/des]\n");
    printf(">  [Perfone asc/des]\n");    
    printf(">  [Perftwo asc/des]\n");   
    printf(">> ");
    scanf("%s", buffer);

    // Check for quit command
    if (buffer[0] == 'q') break;

    if (!charValid(buffer)) continue;

    normalize(buffer);
    scanf("%s", type);

    // Handle term input (mix of char and int)
    if (strcmp(buffer, "term") == 0) {
      for (i = 0; i < table->size; i++) {
        strcpy(arrayS[i], table->list[i].YYT);
      }
      term_sort = 1;
      exec = 1;
    } else if (strcmp(buffer, "ssn") == 0) {
      for (i = 0; i < table->size; i++) {
        arrayI[i] = table->list[i].ssn;
      }
      int_sort = 1;
      exec = 1;
    } else if (strcmp(buffer, "perfone") == 0) {
      for (i = 0; i < table->size; i++) {
        arrayI[i] = table->list[i].p1;
      }
      int_sort = 1;
      exec = 1;
    } else if (strcmp(buffer, "perftwo") == 0) {
      for (i = 0; i < table->size; i++) {
        arrayI[i] = table->list[i].p2;
      }
      int_sort = 1;
      exec = 1;
    } else if (strcmp(buffer, "last") == 0) {
      for (i = 0; i < table->size; i++) {
        strcpy(arrayS[i], table->list[i].last);
      }
      string_sort = 1;
      exec = 1;
    } else if (strcmp(buffer, "first") == 0) {
      for (i = 0; i < table->size; i++) {
        strcpy(arrayS[i], table->list[i].first);
      }
      string_sort = 1;
      exec = 1;
    }
  } while (exec != 1);

  // Execute specific sort 
  if (term_sort == 1) {
    termSort(table, arrayS, type);
  } else if (int_sort == 1) {
    intSort(table, arrayI, type);
  } else if (string_sort == 1) { 
    stringSort(table, arrayS, type);
  }
  *saved = 0;

  printf("Display Sorted Table (Y/N)? >> ");
  scanf("%s", buffer);
  if (buffer[0] == 'Y' || buffer[0] == 'y') displayTable(table);

  free(arrayI);
  free(arrayS);
}

void saveTable(Table *table, int *saved) {
  char buffer[64], *end;
  int i, len = 0;
  FILE *wptr = NULL;

  // Create output file name 
  printf("Enter an output recruit file (example.txt) >> ");
  scanf("%s", buffer);
  // Set expected .txt to end 
  wptr = fopen(buffer, "w");
  if (wptr == NULL) {
    printf("Unable to create file\n");
    return;
  }

  secureData(0, table->size, table, encrypt);
  // Print values to file 
  for (i = 0; i < table->size; i++) {
    
    fprintf(wptr, "%09d %s %s %3s %d %d\n",
      table->list[i].ssn,
      table->list[i].last,
      table->list[i].first,
      table->list[i].YYT,
      table->list[i].p1,
      table->list[i].p2
    );

  }
  fclose(wptr);

  printf("Enter an output encryption file (example.txt) >> ");
  scanf("%s", buffer);
  wptr = fopen(buffer, "w");
  if (wptr == NULL) {
    printf("Unable to create file\n");
    return;
  }

  for (i = 0; i < table->size; i++) 
    fprintf(wptr, "%09d %c\n", table->keys[i].ssn, table->keys[i].val);

  *saved = 1;
}

// Read new data into table
void loadData(Table *table, int *saved) {
  char buffer[64];
  int i, len = 0, exec = 0, running = 0, scan = 0, to_read = 0;
  FILE *rptr = NULL;

  do {
    printf("Enter a new recruit file to read >> ");
    scanf("%s", buffer);
    rptr = fopen(buffer, "r");
    if (rptr == NULL) {
      printf("Invalid File!\n");
      continue;
    }
    exec = 1;
  } while (exec == 0);

  // Read data 
  running = 1;
  while (running == 1) {
    if (table->size == table->max) {
      // Resize arrays to 2x(curr_siz) (Both arrays)
      resize(table, (void *)0);
    } 
    
    fscanf(rptr, "%d", &table->list[table->size].ssn);
    fscanf(rptr, "%s", table->list[table->size].last);
    fscanf(rptr, "%s", table->list[table->size].first);
    fscanf(rptr, "%d", &table->list[table->size].p1);
    scan = fscanf(rptr, "%d", &table->list[table->size].p2);
    if (scan == EOF) {
      running = 0;
    }                 // Check return of last scan for eof
    i++;
    table->size++;
  }
  fclose(rptr);

  do {
    printf("Enter a new key file to read >> ");
    scanf("%s", buffer);
    rptr = fopen(buffer, "r");
    if (rptr == NULL) {
      printf("Invalid File!\n");
      continue;
    }
    exec = 1;
  } while (exec == 0);


  to_read = i;
  // Read from security file into keys array 
  for (i = table->size - to_read; i < table->size; i++) {
    // Scan key into array then scan value after into garbage buffer 
    fscanf(rptr, "%d", &table->keys[i].ssn); 
    fscanf(rptr, "%c", &table->keys[i].val);

  }
  secureData(to_read, table->size, table, decrypt);

  fclose(rptr);
  *saved = 0;
}

int closeTable(Table *table, int saved) {
  // Force Save to File 
  if (saved == 0) saveTable(table, &saved);

  // Free data afterwards
  free(table->list);
  free(table->keys);

  return -1;
}
