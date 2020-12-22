#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif                                        //değişken gibi tanımlamalar yapıldı

#define MAX_BUFFER 512                       // Max alınabilecek karakter sayısı
#define MAX_ARGS 64                            // max alınabilecek parametre
#define ParsCHracther " \t\n\r\a"                     // parçalamada baz alınacak elemanlar
#define README "readme"                        // help file dosyasın bununursa diye name belirlendi

struct shellstatus_st {
    int foreground;                            // Ön planda yürürtebilmek için
    char * infile;                             // giirlen dosyayı tutacak
    char * outfile;                            // çıkış dosyasını tutacak
    char * outmode;                            // çıkışın modunu ayarlaacak
    char * shellpath;                          // shellin full pathi
};
typedef struct shellstatus_st shellstatus;

extern char **environ;

void type_prompt();                                     //prompt yazdırır
void check_command_line(char **, shellstatus *);        //giirş çıkış için komut satırı kontrolü
void errormesage(char *, char *);                       // Hata mesajını föndürür
void pid_exec(char *, shellstatus,char *);              // eş zamanlı çalışmayı sağlar
char * getcwdstr(char *, int);                          // mevcut dosyayı pathini ayarlar
FILE * redirected_op(shellstatus);                      // return required o/p stream
char * stripath(char*);                                 // dosya ismini döndürü
void systemerror_message(char *, char *);                // system ile ilgili hata mesajlarını yazdırır
int flag =0;
/*******************************************************************/

int main (int argc, char * argv[])
{
    FILE * op = stdout;                   // (redirected) o/p stream
    FILE * ip = stdin;                   // batch/keyboard girişler
    char line[MAX_BUFFER];                  // line buffer
    char cwdbuf[MAX_BUFFER];                   // cwd buffer
    char * args[MAX_ARGS];                     // max argüman sayısını belirler
    char ** arg;                               // pointerın adresini adres olarak tutar  
    char * readmepath;                         // readme pathname
    shellstatus status;                        // meccut structure tanımlar
    int execargs;                              // Komutun çalışıp çalışmayacağını belirler
    int i,counter=0;                           // index olarak çalışır

    
    switch (argc) {
        case 1:                                
            break;
        case 2:                                //  batch/script ile başlatılırsa
            if (!(ip = fopen(argv[1],"r"))) {
                systemerror_message("Hatalı Dosya adı girdiniz dosyanızda ki kodlar okunamadı . 3 saniye sonra script ekranına yönlendiriliyorsunuz...", argv[1]);
                sleep(3); 
                ip = stdin;
            } else {
                setbuf(ip, NULL);        
            }
            break;
        default:
            fprintf( stderr, "%s command line error; Max arg sayisi asildi !\n"
                   "usage: %s [<scriptfile>]", stripath(argv[0]),stripath(argv[0]));
            sleep(3);                       
            exit(1);
        }

//Mevcut konumdan komutları uygulamaya başlatır
    status.shellpath = strdup(strcat(strcat(getcwdstr(cwdbuf, sizeof(cwdbuf)),"/"),stripath(argv[0]))); 

    signal(SIGINT, SIG_IGN);       // Ctr+c tuşlamayı engeller
    signal(SIGCHLD ,SIG_IGN);      // zombie çocuk proccesi engeller
    
    while (!feof(ip)) { // dosya bitesi kadar döngü devam eder

        status.foreground = TRUE;// Yapıyı ön planda başlatmaya hazırlar


        if(ip == stdin) 
        { //prombtu bastırırız
            type_prompt();
            fflush(stdout); //tampon belleği temizleriz
        }
        if (fgets(line, MAX_BUFFER, ip ))
        { //dosyada ki ilk satırı alırız
            arg = args;
            *arg++ = strtok(line,ParsCHracther);
            while ((*arg++ = strtok(NULL,ParsCHracther))); //satır boşsa hiçbirşey yapmaz
            counter=0;
            for (i = 0; args[i]!=NULL; ++i)
            {
                if (strcmp(args[i],"")!=0)
                {
                    counter++;
                }
            }

            if (args[0])
            {                 
                execargs = TRUE;           
                check_command_line(args, &status);  //komutu okur

                if (!strcmp(args[0],"cd")) // şayet okunan değer cd olursa path adreslerini değiştrimek için özel tasarlandı
                {
                    if (!args[1]) 
                    {            
                        printf("%s\n", getcwdstr(cwdbuf, sizeof(cwdbuf)));
                    } else 
                    {
                        if (chdir(args[1]))
                         {  
                            systemerror_message("Mevcut konum degistirme erorr",NULL);
                        } else 
                        {
                            strcpy(cwdbuf,"PWD="); 
                            getcwdstr(&cwdbuf[4], sizeof(cwdbuf)-4);
                            if (putenv(strdup(cwdbuf)))
                                systemerror_message("Hatalı DOsya Girinidz ",NULL);
                        }
                    } 
                    execargs = FALSE;        
                }else if (!strcmp(args[0],"quit"))
                {
                    return 0;
                }
                
                else
                {
                    if (execargs)
                    {
                        for (i = 0; i<counter ; )
                        {
                            if (strcmp(args[i],"")==0)
                            {
                                i++; 
                            }
                            else{
                                if ( args[i+1] != NULL)  
                                {
                                    pid_exec(args[i],status,args[i+1]);
                                    i = i+3;
                                }
                                if (args[i+1] == NULL ){
                                    pid_exec(args[i],status,NULL);
                                    i = i+2;  
                                }
                            }
                        }
                    }
                                       //şayet okunan satırda eleman varsa onu çalıştırmak için atar            
                } 
            }
        }
    }
    return 0; 
}

void type_prompt() // prompt yazdırma işlemini yapar
{
    static int firs_time =0;
    if (firs_time == 0 )
    {
        system("clear");     
    }
    firs_time++;
    printf("Rakun@rootShell $>"); //prompt yazısı 

}

void check_command_line(char ** args, shellstatus *sstatus)
{
    sstatus->infile = NULL;      // structda ki değerlere null değerleri atandı
    sstatus->outfile = NULL;     //çıkış dosyasını oluşturmak için yazıldı
    sstatus->outmode = NULL;

    while (*args) {
        if (!strcmp(*args, "<"))  //cat vb komutların çalıştırılmasında kullanılır
        {
            *args = NULL;         
            if(*(++args)) 
            {       
                if(access(*args, F_OK)) {
                    errormesage(*args,"does not exist for i/p redirection."); //yanlşış karakter kulllanımları ifade eder
                } else if (access(*args, R_OK)) {
                    errormesage(*args,"is not readable for i/p redirection.");
                } else {
                    sstatus->infile = *args;
                }
            } else {
                errormesage("no filename with i/p redirection symbol.", NULL);
                break;             
            }   
        } else if (!strcmp(*args, ">") || !strcmp(*args, ">>"))
         {
            if (!strcmp(*args, ">")) { 
                sstatus->outmode = "w"; //eğer dosyaya yazılmak isteniyorsa çıkış modunu değiştirir
            } else
            {		   
                sstatus->outmode = "a";
            }
            *args = NULL;           
            if(*(++args)) {         
                if(!access(*args, W_OK) || access(*args, F_OK)) {
                    sstatus->outfile = *args;
                } else {
                    errormesage(*args,"is not writable/creatable for o/p redirection.");
                }
            } else {
                errormesage("no filename with o/p redirection symbol.", NULL);
                break;
            }
        } else if (!strcmp(*args, ";")) {
            strcpy(*args,"");
            flag = 1;
            //*args = NULL;           
            sstatus->foreground = FALSE; // arka planı restler
        }

        args++;
    }
}

void pid_exec(char * args, shellstatus sstatus,char * paramet)
{
    int status;
    pid_t child_pid;
    char tempbuf[MAX_BUFFER];

    switch (child_pid = fork()) {
        case -1:
            systemerror_message("fork",NULL); // pid değeri -1e eşitse sistem hatası gönderir
            break;
        case 0:
            signal(SIGINT, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);
            
            if (sstatus.infile) { //şayet dosya açıldı ve null değer dğeilşse ve dosya kapalı değilse
                if (!freopen(sstatus.infile, "r", stdin))
                    errormesage(sstatus.infile,"Dosya açık olamaz");
            }
            if (sstatus.outfile) {
                if (!freopen(sstatus.outfile, sstatus.outmode, stdout))
                    errormesage(sstatus.outfile,"Dosya açık olamaz");
            }

            if (putenv(strdup(strcat(strcpy(tempbuf,"PARENT="),sstatus.shellpath))))
                systemerror_message("setting of PARENT environment failed", NULL);

            execlp(args,args,paramet,(char*)NULL);
            systemerror_message("exec failed -",args);
            exit(127); //verilen komut PATHsistem değişkeninizde bulunmadığında ve yerleşik bir kabuk komutu olmadığında döndürülür 
    }

    if (sstatus.foreground) waitpid(child_pid, &status, WUNTRACED); //pidleri bekleme moduna alır
}

char * getcwdstr(char *buffer, int size)
{
    if(!getcwd(buffer, size)) {  //mevcut dosyanın içerğini okur
        systemerror_message("getcwd",NULL);
        buffer[0] = 0;
    }
    return buffer;
}


FILE * redirected_op(shellstatus status)
{
    FILE * ostream = stdout; // dosyanın durumunu tekrar döndüerbilmek için kullanıldı

    if (status.outfile) {
        if (!(ostream = fopen(status.outfile, status.outmode))) {
            systemerror_message(status.outfile,"can't be open for o/p redirection.");
            ostream = stdout;
        }
    }
    return ostream;
}

char * stripath(char * pathname) //path değerini alır
{
    char * filename = pathname;

    if (filename && *filename) {           
        filename = strrchr(filename, '/');
        if (filename)                      
            if (*(++filename))            
                return filename;
            else
                return NULL;
        else
            return pathname;              
    }                                      
    return NULL;
}

void errormesage(char * msg1, char * msg2)  //hata mesajını döndürür
{
    fprintf(stderr,"ERROR: ");
    if (msg1)
        fprintf(stderr,"%s; ", msg1);
    if (msg2)
        fprintf(stderr,"%s; ", msg2);
    return;
    fprintf(stderr,"\n");
}
void systemerror_message(char * msg1, char * msg2) //system hatasını hata mesajı döndüren fonksiyona iletir ve hata nedenini söyler
{
    errormesage(msg1, msg2);
    perror(NULL);
    return;
}
