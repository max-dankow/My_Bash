#include "app_running.h"

const int IS_FIRST = 1;
const int IS_NOT_FIRST = 0;

const char* PROCESS_STATUS_TITLE[] = {"unknown", "terminated", "stopped", "signaled", "no change", "running"};
//process status
const int PS_UNKNOWN = 0;
const int PS_EXITED = 1;
const int PS_STOPED = 2;
const int PS_SIGNALED = 3;
const int PS_NOTHING = 4;
const int PS_RUNNING = 5;

const int FG_IS_ACTIVE = 1;
const int FG_IS_NOT_ACTIVE = 0;

const int RUN_BACKGROUND = 0;
const int RUN_FOREGROUND = 1;

char* generate_process_title(char *const argv[])
{
	char** arg_ptr = argv;
	size_t total_length = strlen(argv[0]);
	while (*arg_ptr != NULL)
	{
		total_length += 1 + strlen(*arg_ptr);
		++arg_ptr;
	}
	char* comand = (char*) malloc(total_length);
	total_length = 0;
	arg_ptr = argv;
	while (*arg_ptr != NULL)
	{
		//total_length += 1;//strlen(*arg_ptr);
		sprintf(comand + total_length, "%s \n", *arg_ptr);
		total_length += strlen(*arg_ptr) + 1;
		++arg_ptr;
	}
	comand[total_length - 1] = '\0';
	return comand;
}

int run_application(int d_in, int d_out, int d_err, const char* app_name, char *const argv[], 
	int* returned_code, JobsList* jobs)
{
  //делаем ответвление для запуска
	pid_t exec_child = fork();
	if (exec_child == -1)
	{
		return -1;
	}
	if (exec_child == 0)
	{
	  //перенаправляем все потоки
		dup2(d_in, 0);
		dup2(d_out, 1);
		dup2(d_err, 2);
  	  //выполняем exec
		if (execvp(app_name, argv) == -1)
		{
			fprintf(stderr, "I can't find this comand: %s\n", app_name);
            fprintf(stderr, "You can try \"help\", but I think it will not help you\n");
			exit(EXIT_FAILURE);
		}
	}
	return 0;
}

int bind_two_apps(int first, int input_fd, int output_fd, const char* app_name, char *const argv[])
{
	int write_fd, read_fd;
  //если это не последняя команда в цепочке, то создаем pipe
	if (output_fd == -1)
	{
		int pipes[2];
		if (pipe(pipes) == -1)
		{
			return -1;
		}
		write_fd = pipes[1];
		read_fd = pipes[0];
	}
  //иначе нужно писать туда, куда сказано(output_fd)
	else
	{
		write_fd = output_fd;
		read_fd = -1;
	}
	if (run_application(input_fd, write_fd, 2, app_name, argv, NULL, NULL) == -1)
	{
		return -2;
	}
  //если это не первая команда в цепочке, то следует закрыть соединяющий входной пайп   
    if (first != IS_FIRST)
    {
    	close(input_fd);
    }
  //если это не последняя команда в цепочке, то следует закрыть выходной пайп 
    if (output_fd == -1)
    {
    	close(write_fd);
    }
    return read_fd;
}

int run_comand_chain(int d_in, int d_out, int d_err, int comand_count, 
	const char** apps_names, char** apps_args[], int* returned_code, JobsList* jobs, int bg_flag)
{
	int next;
	int real_in = d_in;
	int* fake_fd;
	fake_fd = create_fake_discriptor();
	real_in = dup(fake_fd[0]);
	pid_t run_child = fork();
	if (run_child == -1)
	{
		return -1;
	}
	if (run_child == 0)
	{
		for(size_t i = 0; i < comand_count; ++i)
		{
		  //если команда последняя, то писать в d_out, иначе в соединяющий пайп	
			int current_out = (i + 1 == comand_count)? d_out : -1;
		  //если команда первая, то читать из d_in	
			if (i == 0)
			{
				next = bind_two_apps(IS_FIRST, real_in, current_out, apps_names[i], apps_args[i]);
			}
		  //иначе из соединительного пайпа	
			else
			{
				next = bind_two_apps(IS_NOT_FIRST, next, current_out, apps_names[i], apps_args[i]);	
			}
			if (next == -2)
			{
				exit(EXIT_FAILURE);
			}
		}
		
		for (size_t i = 0; i < comand_count; ++i)
		{
			wait(NULL);
		}
		exit(EXIT_SUCCESS);
	}
	else
	{
		char* comand = generate_process_title(apps_args[0]);
		if (bg_flag == RUN_BACKGROUND)
		{
			printf("Job added\n");
			add_job(jobs, run_child, comand, fake_fd, real_in, FG_IS_NOT_ACTIVE);
		}
		else
		{
			//waitpid(run_child, NULL, 0);
			//printf("Job added\n");
			add_job(jobs, run_child, comand, fake_fd, /*real_in*/d_in, FG_IS_ACTIVE);
		}
	}
	return 0;
}

JobsList* init_jobs_system(size_t new_size)
{
	JobsList* new_jobs_list = (JobsList*) malloc(sizeof(JobsList));
	new_jobs_list->list_size = new_size;
	new_jobs_list->jobs_count = 0;
	new_jobs_list->jobs_list_ptr = (JobStruct*) malloc(sizeof(JobStruct) * new_size);
	return new_jobs_list;
}

void delete_jobs_system(JobsList* jobs)
{
	if (jobs == NULL)
	{
		return;
	}
	for (size_t i = 0; i < jobs->jobs_count; ++i)
	{
		free(jobs->jobs_list_ptr[i].comand_str);
	}
	free(jobs->jobs_list_ptr);
	free(jobs);
}

void add_job(JobsList* jobs, pid_t new_pid, char* name, int* real_fake_fd, int new_current_in, int new_bg_flag)
{
	jobs->jobs_list_ptr[jobs->jobs_count].pid = new_pid;
	jobs->jobs_list_ptr[jobs->jobs_count].status = PS_RUNNING;
	jobs->jobs_list_ptr[jobs->jobs_count].comand_str = name;
	jobs->jobs_list_ptr[jobs->jobs_count].fake_fd = real_fake_fd;
	jobs->jobs_list_ptr[jobs->jobs_count].current_input = new_current_in;
	jobs->jobs_list_ptr[jobs->jobs_count].fg_flag = new_bg_flag;
	jobs->jobs_count++;
}

void delete_one_job(JobsList* jobs, size_t job_number)
{
	if (job_number < jobs->jobs_count)
	{
		jobs->jobs_list_ptr[job_number].fg_flag = FG_IS_NOT_ACTIVE;
		free(jobs->jobs_list_ptr[job_number].comand_str);
		delete_fake_discriptor(jobs->jobs_list_ptr[job_number].fake_fd);
	}
}

int update_process_status(JobStruct* job, int* additional_info)
{
	//kill(job->pid, SIGSTOP);
	int process_info;
	int wait_code = waitpid(job->pid, &process_info, WNOHANG | WUNTRACED | WCONTINUED);
	if (wait_code == -1)
	{
		return -1;
	}
	*additional_info = 0;
	if (wait_code == job->pid)
	{
		if (WIFEXITED(process_info))
		{
			*additional_info = WEXITSTATUS(process_info);
			job->status = PS_EXITED;
			job->fg_flag = FG_IS_NOT_ACTIVE;
		}
		if (WIFSTOPPED(process_info))
		{
			job->status = PS_STOPED;
			job->fg_flag = FG_IS_NOT_ACTIVE;
		}
		if (WIFSIGNALED(process_info))
		{
			*additional_info = WTERMSIG(process_info);
			job->status = PS_SIGNALED;
		}
		if (WIFCONTINUED(process_info))
		{
			job->status = PS_RUNNING;
		}
	}
	return 0;
}

void show_jobs(JobsList* jobs)
{
	printf("active  - %d\n", get_active_pid(jobs));
	//printf("Jobs(%d)[%p]\n", jobs->jobs_count, jobs);
	for (size_t i = 0; i < jobs->jobs_count; ++i)
	{
		int additional_info;
		if (update_process_status(&(jobs->jobs_list_ptr[i]), &additional_info) == -1)
		{
			//delete_one_job(jobs, i);
			continue;
		}
		char activity = (jobs->jobs_list_ptr[i].fg_flag == FG_IS_ACTIVE)? '+' : ' ';
		if (jobs->jobs_list_ptr[i].status == PS_EXITED)
		{
			printf("[%d]%c %d %s with code %d (%s)\n", i, activity, jobs->jobs_list_ptr[i].pid, 
				PROCESS_STATUS_TITLE[jobs->jobs_list_ptr[i].status], additional_info, 
				jobs->jobs_list_ptr[i].comand_str);
			continue;
		}
		if (jobs->jobs_list_ptr[i].status == PS_SIGNALED)
		{
			printf("[%d]%c %d %s by signal %d (%s)\n", i, activity, jobs->jobs_list_ptr[i].pid, 
				PROCESS_STATUS_TITLE[jobs->jobs_list_ptr[i].status], additional_info, 
				jobs->jobs_list_ptr[i].comand_str);
			continue;
		}
		printf("[%d]%c %d %s (%s)\n", i, activity, jobs->jobs_list_ptr[i].pid,
			PROCESS_STATUS_TITLE[jobs->jobs_list_ptr[i].status], jobs->jobs_list_ptr[i].comand_str);	
	}
}

int* create_fake_discriptor(void)
{
	int* new_fd = (int*) malloc(2 * sizeof(int));
	pipe(new_fd);
	return new_fd;
}

void delete_fake_discriptor(int* fd)
{
	close(fd[0]);
	close(fd[1]);
	free(fd);
}

int signal_process(JobsList* jobs, size_t job_number, int signal_to_send)
{
	if (job_number < jobs->jobs_count)
	{
		kill(jobs->jobs_list_ptr[job_number].pid, signal_to_send);
		return 0;
	}
	else
	{
		return -1;
	}
}

int stop_process(JobsList* jobs, size_t job_number)
{
	if (signal_process(jobs, job_number, SIGSTOP) == 0)
	{
		printf("Process stoped: %s\n", jobs->jobs_list_ptr[job_number].comand_str);
		return 0;
	}
	return -1;
}

int continue_process(JobsList* jobs, size_t job_number)
{
	if (signal_process(jobs, job_number, SIGCONT) == 0)
	{
		printf("Process continued: %s\n", jobs->jobs_list_ptr[job_number].comand_str);
		return 0;
	}
	return -1;
}

int process_to_foreground(JobsList* jobs, size_t job_number)
{
	if (jobs->jobs_count > job_number)
	{
	  //отдаем процессу stdin		
		dup2(0, jobs->jobs_list_ptr[job_number].current_input);
		jobs->jobs_list_ptr[job_number].fg_flag = FG_IS_ACTIVE;	
		continue_process(jobs, job_number);
		int process_info;
		show_jobs(jobs);
		waitpid(jobs->jobs_list_ptr[job_number].pid, &process_info, 0);
	}
	else
	{
		return -1;
	}
}

pid_t get_active_pid(JobsList* jobs)
{
	pid_t active_pid = getpid();
	for (size_t i = 0; i < jobs->jobs_count; ++i)
	{
		int additional_info;
		if (update_process_status(&(jobs->jobs_list_ptr[i]), &additional_info) == -1)
		{
			//delete_one_job(jobs, i);
			continue;
		}
		if (jobs->jobs_list_ptr[i].fg_flag == FG_IS_ACTIVE)
		{
			active_pid = jobs->jobs_list_ptr[i].pid;
			break;
		}
	}
	return active_pid;
}