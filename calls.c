#include "calls.h"

int maxCallLen = 512;

extern char** environ;

void strspcpy(char* output, const char* from)
{
    int i = 0;
    while (from[i] > ' ')
    {
        output[i] = from[i];
        ++i;
    }
    output[i] = 0;
}
int getword(const char* stream, char* output)
{
    int i = 0;
    int st = 0;
    while (stream[st] != 0 &&
        !((stream[st] >= 'a' && stream[st] <= 'z') ||
          (stream[st] >= 'A' && stream[st] <= 'Z') ||
          (stream[st] >= '0' && stream[st] <= '9')))
          ++st;
    if (stream[st] == 0)
        return -1;
    if (st > 0 && stream[st - 1] == '$')
    {
        --st;
        output[0] = '$';
        ++i;
    }
    while((stream[st + i] >= 'a' && stream[st + i] <= 'z') ||
          (stream[st + i] >= 'A' && stream[st + i] <= 'Z') ||
          (stream[st + i] >= '0' && stream[st + i] <= '9'))
    {
        output[i] = stream[st + i];
        ++i;
    }
    if (i < 1)
        return -1;
    output[i] = 0;
    return 0;
}
int progSepar(char ch)
{
    if (ch != '|' && ch != '&' && ch != 0 && ch != '(' && ch != ')' && ch != '>')
        return -1;
    else
        return 0;
}
int oneProgPars(char*** output, const char* callstr)
{
    while (progSepar(*callstr) == -1 && *callstr <= ' ')
        ++callstr;
    int argc = 0;
    int i = 0;
    while (progSepar(callstr[i]) != 0)
    {
        while (callstr[i] > ' ')
        {
            if (callstr[i] == '\"' && (i == 0 || callstr[i - 1] != '\\'))
            {
                ++i;
                while (callstr[i] != '\"' && progSepar(callstr[i]) != 0)
                    ++i;
            }
            ++i;
        }
        while (progSepar(callstr[i]) != 0 && callstr[i] <= ' ')
            ++i;
        ++argc;
    }
    *output = (char**)malloc((argc + 1)*sizeof(char*));
    (*output)[argc] = NULL;
    int argnmb = 0;
    i = 0;
    while (progSepar(callstr[i]) != 0)
    {
        int arglen = 0;
        int firstlen = 0;
        int reallen = 0;
        char forbindArg[256];
        while (callstr[i + firstlen] > ' ')
        {
            if (callstr[i + firstlen] == '"' && (i + firstlen == 0 || callstr[i + firstlen - 1] != '\\'))
            {
                ++firstlen;
                while (callstr[i + firstlen] != '"' && progSepar(callstr[i + firstlen] != 0))
                {
                    forbindArg[reallen] = callstr[i + firstlen];
                    ++firstlen;
                    ++reallen;
                }
                if (progSepar(callstr[i + firstlen]) != 0)
                    ++firstlen;
            }
            else
            {
                forbindArg[reallen] = callstr[i + firstlen];
                ++firstlen;
                ++reallen;
            }
        }
        forbindArg[reallen] = 0;
        int j = 0;
        char varenv[256];
        arglen = 0;
        for (j = 0; j < reallen; ++j)
        {
            if (forbindArg[j] == '$' && getword(forbindArg + j + 1, varenv) == 0 && getenv(varenv) != NULL)
            {
                arglen = arglen + strlen(getenv(varenv));
                j = j + strlen(varenv) + 1;
            }
            else
                ++arglen;
        }
        (*output)[argnmb] = (char*)malloc((arglen + 1)*sizeof(char));
        arglen = 0;
        for (j = 0; j < reallen; ++j)
        {
            if (forbindArg[j] == '$' && getword(forbindArg + j + 1, varenv) == 0 && getenv(varenv) != NULL)
            {
                strcpy((*output)[argnmb] + arglen, getenv(varenv));
                arglen = arglen + strlen(getenv(varenv));
                j = j + strlen(varenv);
            }
            else
            {
                (*output)[argnmb][arglen] = forbindArg[j];
                ++arglen;
            }
        }
        (*output)[argnmb][arglen] = 0;
        //printf("ps %d: %s\n", argnmb, (*output)[argnmb]);
        i = i + firstlen;
        while (progSepar(callstr[i]) != 0 && callstr[i] <= ' ')
            ++i;
        ++argnmb;
    }
    return argc;
}

int oneStrCall(const char* callstr, char* path, JobsList* jobs)
{
    int len = strlen(callstr);
    if (len < 1)
        return 0;
    char comName[256];
    int i = 0;
    while (callstr[i] > ' ')
        ++i;
    strncpy(comName, callstr, i);
    comName[i] = 0;
    if (strcmp(comName, "exit") == 0) //и аналогично для всех внутренних команд
    {
        /*int i = 0;
        printf("\n");
        appendHistory(newhist, newhistCount);
        for (i = 0; i < 6; ++i)
        {
            printf("\033[%dm*\033[0m", 31 + i);
            fflush(stdout);
        }
        printf("\nGoodbye.\n");
        return 0;*/
        return -2;
    }
    else if (strcmp(comName, "help") == 0)
    {
        printf("Some helpless information about e-bash:\n");
        printf("internal comands:\n");
        printf("\texit - exit from e-bash:(\n");
        printf("\thelp - you are reading it now\n");
        printf("\tcd <path> - change directory to path\n");
        printf("\tjobs - print list of jobs - child processes\n");
        printf("\tjobfg <nmb> - resume job nmb in foreground\n");
        printf("\tjobbg <nmb> - resume job nmb in background\n");
        printf("\tjobstop <nmb> - stop job nmb to background\n");
        printf("\tjobsig <nmb> <signal> - send signal to job process\n");
        printf("\tenvvar [name = value] - see envvar list or set name as value\n");
        printf("\te-bash <scriptfile.ebs> - launch e-bash script\n");
        printf("syntax of other calls:\n");
        printf("\t[path/]<programm> [args...] ['|'<samecall>] [&] [> output|< input]\n");
        printf("\tit is execute path/program (or paths from $PATH)\n");
        printf("\twhere samecall:[path/]<programm> [args...] [|<samecall>]\n");
        printf("\tand | it is conveyer chain, & - background launch\n");
        printf("WARNING: developers of e-bash guarantee nothing!\n");
        printf("If you ignore this syntax rules, anything may happen!\n");
        printf("(It is possible to teleport to pole or start nuclear war by wrong syntax)\n");
        return 0;
    }
    else if (strcmp(comName, "cd") == 0)
    {
        if (callstr[2] != ' ')
        {
            printf("use cd as \"cd <path>\" or same\n");
            printf("You can try \"help\", but I think it will not help you\n");
            return -1;
        }
        char newDir[256];
        int i = 2;
        while (callstr[i] <= ' ' && callstr[i] != 0)
            ++i;
        strcpy(newDir, callstr + i);
        int code = chdir(newDir);
        if (code == 0)
        {
            getcwd(path, 255);
            printf("\033]2;e-bash in %s\007", path);
            fflush(stdout);
        }
        return code;
    }
    else if (strcmp(comName, "jobs") == 0)
    {
        show_jobs(jobs);
        return 0;
    }
    else if (strcmp(comName, "jobsig") == 0)
    {
        int nmb;
        int sig;
        sscanf(callstr, "jobsig%d%d", &nmb, &sig);
        return signal_process(jobs, nmb, sig);
    }
    else if (strcmp(comName, "jobbg") == 0)
    {
        int nmb;
        sscanf(callstr, "jobbg%d", &nmb);
        return continue_process(jobs, nmb);
    }
    else if (strcmp(comName, "jobfg") == 0)
    {
        int nmb;
        sscanf(callstr, "jobfg%d", &nmb);
        return process_to_foreground(jobs, nmb);
    }
    else if (strcmp(comName, "jobstop") == 0)
    {
        int nmb;
        sscanf(callstr, "jobstop%d", &nmb);
        return stop_process(jobs, nmb);
    }
    else if (strcmp(comName, "envvar") == 0)
    {
        char varname[32];
        char value[256];
        if (getword(callstr + 6, varname) != -1 && getword(callstr + 7 + strlen(varname) + 1, value) != -1)
        {
            if (value[0] == '$')
            {
                char* repl = getenv(value + 1);
                if (repl != NULL)
                    strcpy(value, repl);
            }
            return setenv(varname, value, 1);
        }
        int i = 0;
        while (environ[i] != NULL)
        {
            printf("%s\n", environ[i]);
            ++i;
        }
        return 0;
    }
    /*else if (strcmp(comName, "ebs"))
    {
        char** argvr;
        oneProgPars(&argvr, callstr + 4);
        scriptRunner(0, 1, argvr);
    }*/
    else
    {
        //sprintf("%s isn't a internal comand\n", comName);

        int code = 0;
        int i = 0;
        //int len = strlen(callstr);
        char*** allsargv = NULL;
        char** progNames = NULL;
        int progCount = 1;
        char* nextProg = callstr;
        int infd = 0;
        int outfd = 1;
        JobsList* BGflag = NULL;
        while(callstr[i] != 0)
        {
            if (callstr[i] == '|')
                ++progCount;
            ++i;
        }
        allsargv = (char***)malloc((progCount)*sizeof(char**));
        progNames = (char**)malloc((progCount)*sizeof(char*));
        for (i = 0; i < progCount; ++i)
        {
            oneProgPars(allsargv + i, nextProg);
            progNames[i] = (char*)malloc((strlen(allsargv[i][0]) + 1)*sizeof(char));
            strcpy(progNames[i], allsargv[i][0]);
            progNames[i][strlen(allsargv[i][0])] = 0;
            while(progSepar(*nextProg) != 0)
                ++nextProg;
            if (*nextProg == '>')
            {
                char filename[256];
                sscanf(nextProg, ">%s", filename);
                if (filename[0] == '$')
                {
                    filename[0] = '/';
                    outfd = shm_open(filename, O_RDWR | O_CREAT, 0666);
                }
                outfd = open(filename, O_RDWR | O_CREAT, 0666);
            }
            else if (*nextProg == '<')
            {
                char filename[256];
                sscanf(nextProg, "<%s", filename);
                if (filename[0] == '$')
                {
                    filename[0] = '/';
                    infd = shm_open(filename, O_RDWR | O_CREAT, 0666);
                }
                infd = open(filename, O_RDWR | O_CREAT, 0666);
            }
            else if (*nextProg == '&')
                BGflag = jobs;
            while(*nextProg != 0 && (progSepar(*nextProg) == 0 || *nextProg <= ' '))
                ++nextProg;
        }
        if (run_comand_chain(infd, outfd, 2, progCount, progNames, allsargv, &code, BGflag) != 0)
        {
            printf("I can't find this comand: %s\n", comName);
            printf("You can try \"help\", but I think it will not help you\n");
            return -1;
        }
        for (i = 0; i < progCount; ++i)
        {
            int j = 0;
            while(allsargv[i][j] != NULL)
            {
                free(allsargv[i][j]);
                ++j;
            }
            free(progNames[i]);
            free(allsargv[i]);
        }
        return code;
    }
}

int parsOneBrakes(const char* callstr, char* oper, char* first, char* second)
{
    first[0] = 0;
    second[0] = 0;
    int pos = 0;
    while (callstr[pos] != '(')
        ++pos;
    ++pos;
    while (callstr[pos] == ' ')
        ++pos;
    while (callstr[pos] != ' ' && callstr[pos] != '=' && callstr[pos] != '!' && callstr[pos] != '<')
    {
        strncat(first, callstr + pos, 1);
        ++pos;
    }
    while (callstr[pos] == ' ')
        ++pos;
    *oper = callstr[pos];
    ++pos;
    while (callstr[pos] == ' ')
        ++pos;
    while (callstr[pos] != ' ' && callstr[pos] != ')')
    {
        strncat(second, callstr + pos, 1);
        ++pos;
    }
    while (callstr[pos] != ')')
        ++pos;
    ++pos;
    return pos;
}

int scriptRunner(char** argv)
{
    char* filename = argv[1];
    int scriptfd = open(filename, O_RDONLY);
    if (scriptfd < 0)
    {
        fprintf(stderr, "Error: script file %s does not exist.\n", filename);
        return -1;
    }
    JobsList* jobs = init_jobs_system(50);
    struct stat st;
    fstat(scriptfd, &st);
    char* script = (char*)mmap(NULL, sizeof(char)*st.st_size, PROT_READ, MAP_SHARED, scriptfd, 0);
    int i = 0;
    while (argv[i] != NULL)
    {
        char argstr[10];
        sprintf(argstr, "arg%d", i);
        setenv(argstr, argv[i], 1);
        ++i;
    }
    char argcstr[10];
    sprintf(argcstr, "%d", i);
    setenv("argc", argcstr);
    char path[256];
    getcwd(path, sizeof(path));
    while (i < st.st_size)
    {
        char callstr[maxCallLen];
        char comName[32];
        int j = i;
        while (j < st.st_size && script[j] != '\n' && script[j] != 0)
            ++j;
        strncpy(callstr, script + i, j - i);
        callstr[j - i] = 0;
        i = j + 1;
        getword(callstr, comName);
        if (strcmp(comName, "exit") == 0)
        {
            int retcode = -1;
            sscanf(callstr, "exit%d", &retcode);
            return retcode;
        }
        if (strcmp(comName, "if") == 0)
        {
            char first[32];
            char second[32];
            char oper;
            int endpos = parsOneBrakes(callstr + 2, &oper, first, second);
            if (oper == '=' && strcmp(first, second) == 0)
            {
                oneStrCall(callstr + endpos + 1, path, jobs);
            }
            else
                continue;
        }
        oneStrCall(callstr, path, jobs);
    }
    close(scriptfd);
    return 0;
}