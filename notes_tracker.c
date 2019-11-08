#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mysql.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

typedef struct note {
  char* name;
  char* description;
  struct note* next;
} note_t;

char flask_ip[32] = {0};
char database_ip[32] = {0};

note_t* notes_list;

void print_menu() {
  printf("\n========= Notes Tracker Menu =========\n");
  printf("1. List Notes\n");
  printf("2. Add/Edit Note\n");
  printf("3. Delete Notes\n");
  printf("4. Get Notes from database\n");
  printf("5. Commit to database\n");
  printf("6. Save database backup to external server\n");
  printf("7. Restore database backup from external server\n");
  printf("8. Exit\n");
  printf("Please make your selection: ");
}

void print_notes() {
  note_t* curr = notes_list;
  int i = 1;
  printf("========= Current Noted =========\n");
  if(curr == NULL) {
    printf("None (your notes is currently empty)\n");
  }

  while(curr != NULL) {
    printf("%d. %s (description: %s)\n", i, curr->name, curr->description);
    curr = curr->next;
    i++;
  }
}

note_t* new_note(char* name, char* description) {
  note_t* ni = (note_t*)malloc(sizeof(struct note));

  ni->name = name;
  ni->description = description;
  ni->next = NULL;

  return ni;
}

void add_edit_note() {
  char inp[100] = {0};
  char* name = NULL;
  char inp2[100] = {0};
  char* description = NULL;
  note_t* curr = notes_list;
  int already_in_list = 0;

  printf("Input note name: ");
  gets(inp);
  strtok(inp, "\n");
  printf("Input note description: ");
  gets(inp2);
  strtok(inp2, "\n");
  
  description = (char*)malloc(strlen(inp2));
  strcpy(description, inp);

  while(curr != NULL) {
    if(strcmp(inp, curr->name) == 0) {
      curr->description = description;
      already_in_list = 1;
      break;
    }
    curr = curr->next;
  }
  if (already_in_list) {
    printf("Editing note ");
  }
  else {
    printf("Adding note ");
  }
  printf(inp);
  printf(" with description ");
  printf(inp2);

  if(!already_in_list) {
    name = (char*)malloc(64);
    strcpy(name, inp);
    note_t* ni = new_note(name, description);
    curr = notes_list;
    if(notes_list == NULL) {
      notes_list = ni;
    } else {
      while(curr->next != NULL) {
        curr = curr->next;
      }
      curr->next = ni;
    }
  }
}

void delete_note() {
  char inp[64] = {0};
  note_t* curr = notes_list;
  int found_in_list = 0;
  printf("Input note you would like to remove: ");
  gets(inp);
  strtok(inp, "\n");
  while(curr != NULL) {
    if(strcmp(curr->name, inp) == 0) {
      free(curr);
      printf("Deleted note %s with description %s.\n", curr->name, curr->description);
      found_in_list = 1;
      break;
    }
    curr = curr->next;
  }

  if(!found_in_list) {
    printf("Could not find note %s in notes list. Please try again.\n", inp);
  }
}

void get_notes_from_db(MYSQL* con) {
  const char* query = "SELECT * FROM Notes";
  if(mysql_query(con, query)) {
    printf("Getting current notes from database failed: %s\n", mysql_error(con));
    return;
  }

  MYSQL_RES* result = mysql_store_result(con);

  MYSQL_ROW row;
  int i = 1;
  while((row = mysql_fetch_row(result))) {
    printf("%d. %s (description: %s)\n", i, row[0], row[1]);
    i++;
  }

}

int commit_db(MYSQL* con) {
  note_t* curr = notes_list;
  char query_str[1024] = {0};
  while(curr != NULL) {

    sprintf(query_str, "INSERT INTO Notes VALUES('%s', '%s')", curr->name, curr->description);
    if(mysql_query(con, query_str)) {
      printf("Error executing query: %s.\n", query_str);
      return -1;
    }
    memset(query_str, 0, 1024);
    curr = curr->next;
  }
  return 0;
}

int backup_db() {
    char command[100] = {0};
    char command2[100] = {0};
    char command3[100] = {0};
    char command4[100] = {0};
    char inp[100] = {0};

    printf("What database do you want to dump? ");
    gets(inp);
    sprintf(command, "mysqldump -h %1$s -u notesuser -ppassword %2$s > %2$s.sql", database_ip, inp);
    sprintf(command2, "zip %1$s.zip %1$s.sql", inp);
    sprintf(command3, "curl -F \"file=@%s.zip\" http://%s:3000/uploads", inp, flask_ip);
    sprintf(command4, "rm %1$s.zip %1$s.sql", inp);
    int code = 0;
    code |= system(command);
    code |= system(command2);
    code |= system(command3);
    code |= system(command4);
    if (!code) {
        printf("successfully backed up\n");
    }
    else {
        printf("An error occured\n");
    }
    return 0;
}

int restore_db() {
    char command[100] = {0};
    char command2[100] = {0};
    char command3[100] = {0};
    char inp[100] = {0};

    printf("What database do you want to restore? ");
    gets(inp);
    sprintf(command, "curl http://%1$s:8000/%2$s.sql --output %2$s.sql", flask_ip, inp);
    sprintf(command2, "mysql -h %1$s -u notesuser -ppassword %2$s < %2$s.sql", database_ip, inp);
    sprintf(command3, "rm %1$s.sql", inp);
    int code = 0;
    code |= system(command);
    code |= system(command2);
    code |= system(command3);
    if (!code) {
        printf("successfully backed up\n");
    }
    else {
        printf("An error occured\n");
    }
    return 0;
}

int main(int argc, char* argv[]) {
  char inp[8] = {0};
  notes_list = NULL;
  int fd;
  struct ifreq ifr;

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
  ioctl(fd, SIOCGIFADDR, &ifr);
  close(fd);
  sprintf(database_ip, "%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
  sprintf(flask_ip, "%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
  int dot_count = 0;
  int i = 0;
  for(; i < 32; i++) {
    if(database_ip[i] == '.') {
      dot_count++;
    }
    if(dot_count == 3) {
      database_ip[i+1] = '8';
      database_ip[i+2] = '\0';
      flask_ip[i+1] = '5';
      flask_ip[i+2] = '\0';
      break;
    }
  }


  MYSQL *con = mysql_init(NULL);
  if(con == NULL) {
    printf("Error setting up database. Exiting.\n");
    return -1;
  }
  if(!mysql_real_connect(con, database_ip, "notesuser", "password", "notes", 0, NULL, 0)) {
    printf("Error setting up database. Exiting. Invalid Credentials\n");
    return -1;
  }

  while(1) {
    print_menu();
    fgets(inp, 8, stdin);
    printf("You selected option: %c\n", inp[0]);
    if(inp[0] == '1') {
      print_notes();
    } else if(inp[0] == '2') {
      add_edit_note();
    } else if(inp[0] == '3') {
      delete_note();
    } else if(inp[0] == '4') {
      get_notes_from_db(con);
    } else if(inp[0] == '5') {
      if(commit_db(con)) {
        printf("Errors committing to database. Please check your database connection.\n");
      } else {
        printf("Successfully committed notes to database. Exiting.\n");
        break;
      }
    } else if(inp[0] == '6') {
      backup_db();
    } else if(inp[0] == '7') {
      restore_db();
    } else if(inp[0] == '8') {
      if(notes_list != NULL) {
        printf("You have unsaved changes.");
      }
      printf("Are you sure you want to exit? (y/N) ");
      fgets(inp, 8, stdin);
      if(inp[0] == 'y' || inp[0] == 'Y') {
        break;
      }
    } else {
      printf("Invalid selection. Please try again.\n\n");
    }
  }
  
  mysql_close(con);
  return 0; 
}
