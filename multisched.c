#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <sys/wait.h>

#define MSG(x...) fprintf (stderr, x)
#define STRERROR  strerror (errno)

#define TASK_MAX (26 * 10)
#define ID_MAX 2

#define ARRIVE_TIME_MIN 0
#define ARRIVE_TIME_MAX 30

#define SERVICE_TIME_MIN 1
#define SERVICE_TIME_MAX 30

#define PRIORITY_MIN 1
#define PRIORITY_MAX 10


typedef enum
{
	SCHED_PR,
	SCHED_SJF,
	SCHED_FCFS,
} PROC;    

typedef struct _Task Task;
struct _Task
{
	int    idx;
	int    queue_idx;

	char   id[ID_MAX + 1];
	PROC   process_type;
	int    arrive_time;
	int    service_time;
	int    priority;

	int    remain_time;
	int    complete_time;
	int    turnaround_time;
	int    wait_time;

	int    execute[100];
	int    location;
	int    service_location;
	int    sequence;
};


/* multiqueue */
typedef struct _frontier frontier;
struct _frontier
{
	Task set[TASK_MAX];
	int size;
};

static int check_tasks[TASK_MAX];

	static char *
strstrip (char *str)
{
	char  *start;
	size_t len;

	len = strlen (str);
	while (len--)
	{
		if (!isspace (str[len]))
			break;
		str[len] = '\0';
	}

	for (start = str; *start && isspace (*start); start++)
		;
	memmove (str, start, strlen (start) + 1);

	return str;
}

	static int
check_valid_id (const char *str)
{
	size_t len;
	int    i;

	len = strlen (str);
	if (len != ID_MAX)
		return -1;

	for (i = 0; i < len; i++)
		if (!(isupper (str[i]) 
					|| isdigit (str[i])))
			return -1;

	return 0;
}

	static int 
lookup_id(const char *str, int write_check)
{
	int string = (str[0] - 'A');
	int number = (str[1] - '0');

	int id_check = (string * 10) + number;

	if(write_check)
	{
		if(check_tasks[id_check])
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		check_tasks[id_check] = 1;
		return 1;
	}
}

	void 
swap(frontier *temp_queue, int x, int y)
{
	Task temp_task;

	temp_task = temp_queue->set[x];
	temp_queue->set[x] = temp_queue->set[y];
	temp_queue->set[y] = temp_task;
}

	int
check_empty_queue(frontier *queue)
{
	if(queue->size == 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

	int
queue_push(frontier *queue, Task task)
{
	int current = queue->size;
	int parent = (queue->size - 1) /2;

	if(queue->size > TASK_MAX - 1)
	{
		return 0;
	}
	queue->set[queue->size] = task;

	while((current > 0) 
			&& (queue->set[current].location <= queue->set[parent].location))
	{
		swap(queue, current, parent);
		current = parent;
		parent = (parent -1) / 2;
	}
	queue->size++;

	return 1;
}

	Task
queue_pop(frontier *queue)
{
	Task task = queue->set[0];

	int current = 0;
	int left_index = (current * 2) + 1;
	int right_index = (current * 2) + 2;
	int end = current;

	queue->size--;

	queue->set[0] = queue->set[queue->size];
	while(left_index < queue->size)
	{
		if(queue->set[end].location > queue->set[left_index].location  )
		{
			end = left_index;
		}
		if((right_index < queue->size)
				&& (queue->set[end].location > queue->set[right_index].location))
		{
			end = right_index;
		}
		if(end == current)
		{
			break;
		}
		else
		{
			swap(queue, current, end);
			current = end;
			left_index = (current * 2) + 1;
			right_index = (current * 2) + 2;
		}
	}

	return task;
}




	static int
read_config (const char *filename, frontier *PR, frontier *SJF, frontier *FCFS)
{
	FILE *fp;
	char  line[256];
	int   line_nr;

	fp = fopen (filename, "r");
	if (!fp)
		return -1;

	line_nr = 0;
	while (fgets (line, sizeof(line), fp))
	{
		Task  task;
		char  *p;
		char  *s;
		size_t len;

		line_nr++;
		memset (&task, 0x00, sizeof(task));

		len = strlen (line);
		if (line[len - 1] == '\n')
			line[len - 1] = '\0';

		strstrip (line);

		/* comment or empty line */
		if (line[0] == '#' || line[0] == '\0')
		{
			continue;
		}

		/* id */
		s = line;
		p = strchr (s, ' ');
		if (!p)
		{
			goto invalid_line;
		}
		*p = '\0';
		strstrip (s);
		if (check_valid_id (s))
		{
			MSG ("invalid task id '%s' in line %d, ignored\n", s, line_nr);
			continue;
		}
		if (lookup_id (s, 1))
		{
			MSG ("duplicate task id '%s' in line %d, ignored\n", s, line_nr);
			continue;
		}
		strcpy (task.id, s);

		/* process type */
		s = p + 1;
		p = strchr (s, ' ');
		if(!p)
		{
			goto invalid_line;
		}
		*p = '\0';
		strstrip (s);

		if(!strcmp(s, "H")){
			task.process_type = SCHED_PR;
		}
		else if(!strcmp(s, "M")){
			task.process_type = SCHED_SJF;
		}
		else if(!strcmp(s, "L")){
			task.process_type = SCHED_FCFS;
		}
		else{
			MSG ("invalid process type '%s' in line %d, ignored\n", s, line_nr); 
		}

		/* arrive time */
		s = p + 1;
		p = strchr (s, ' ');
		if (!p)
		{
			goto invalid_line;
		}
		*p = '\0';
		strstrip (s);

		task.arrive_time = strtol (s, NULL, 10);
		if ((task.arrive_time < ARRIVE_TIME_MIN) 
				|| (ARRIVE_TIME_MAX < task.arrive_time))
		{
			MSG ("invalid arrive-time '%s' in line %d, ignored\n", s, line_nr);
			continue;
		}

		/* service time */
		s = p + 1;
		p = strchr (s, ' ');
		if (!p)
			goto invalid_line;
		*p = '\0';
		strstrip (s);
		task.service_time = strtol (s, NULL, 10);
		if (task.service_time < SERVICE_TIME_MIN
				|| SERVICE_TIME_MAX < task.service_time)
		{
			MSG ("invalid service-time '%s' in line %d, ignored\n", s, line_nr);
			continue;
		}

		/* priority */
		s = p + 1;
		strstrip (s);
		task.priority = strtol (s, NULL, 10);
		if (task.priority < PRIORITY_MIN
				|| PRIORITY_MAX < task.priority)
		{
			MSG ("invalid priority '%s' in line %d, ignored\n", s, line_nr);
			continue;
		}

		if(task.process_type == SCHED_PR)
		{
			task.location = task.arrive_time;
			task.sequence = task.arrive_time;
			task.service_location = task.service_time;
			queue_push(PR, task);
			lookup_id(task.id,0);
		}
		else if(task.process_type == SCHED_SJF)
		{
			task.location = task.arrive_time;
			task.sequence = task.arrive_time;
			task.service_location = task.service_time;
			queue_push(SJF, task);
			lookup_id(task.id,0);
		}
		else if(task.process_type == SCHED_FCFS)
		{
			task.location = task.arrive_time;
			task.sequence = task.arrive_time;
			task.service_location = task.service_time;
			queue_push(FCFS, task);
			lookup_id(task.id,0);
		}
		else
		{
			goto invalid_line;
		}

invalid_line:
		MSG ("invalid format in line %d, ignored\n", line_nr);
	}

	fclose (fp);

	return 0;
}

	void
simulate (frontier *PR, frontier *SJF, frontier *FCFS){

	Task task;

	frontier wait_PR;
	frontier wait_SJF;
	frontier queue;

	int select = 0;
	int total_queue_size = PR->size + SJF->size + FCFS->size;

	int      cpu_time = 0;
	int      sum_turnaround_time = 0;
	int      sum_waiting_time = 0;
	float    avg_turnaround_time;
	float    avg_waiting_time;

	int H_assigned_time;
	int M_assigned_time;

	int i = 0;

	while(!(check_empty_queue(&wait_PR) 
				&& check_empty_queue(PR) 
				&& check_empty_queue(&wait_SJF) 
				&& check_empty_queue(SJF)))
	{
		if(!select){
			H_assigned_time = 0;
			while(H_assigned_time < 6)
			{
				if(check_empty_queue(&wait_PR) 
						&& check_empty_queue(PR))
				{
					break;
				}

				for(i = 0; i< PR->size; i++)
				{
					if(PR->set[i].sequence <= 0)
					{
						task = queue_pop(PR);
						task.location = task.priority;
						queue_push(&wait_PR,task);
					}
					else
					{
						break;
					}
				}

				task = queue_pop(&wait_PR);
				task.service_location--;
				task.execute[cpu_time] = 1;
				H_assigned_time++;
				cpu_time++;

				for(i = 0; i < PR->size; i++)
				{
					PR->set[i].sequence--;
				}
				for(i = 0; i < SJF->size; i++)
				{
					SJF->set[i].sequence--;
				}

				if(task.service_location)
				{
					queue_push(&wait_PR,task);
				}
				else
				{
				/*	printf("%s: complete = %d, arrive = %d, service = %d, waiting = %d\n",
							task.id, 
							cpu_time, 
							task.arrive_time, 
							task.service_time,	
							(cpu_time - task.arrive_time) - task.service_time);
*/
					sum_turnaround_time += cpu_time - task.arrive_time;
					sum_waiting_time += (cpu_time - task.arrive_time) - task.service_time;
					task.location = task.arrive_time;
					queue_push(&queue, task);
				}
			}
			select = 1;
		}
		else
		{
			M_assigned_time = 0;
			while(M_assigned_time < 4)
			{
				if(check_empty_queue(SJF) 
						&& check_empty_queue(&wait_SJF))
				{
					break;
				}

				for(i = 0; i < SJF->size; i++)
				{
					if(SJF->set[i].sequence <= 0)
					{
						task = queue_pop(SJF);
						task.location = task.service_time;
						queue_push(&wait_SJF,task);
					}
					else
					{
						break;
					}
				}

				task = queue_pop(&wait_SJF);
				task.service_location--;
				task.execute[cpu_time] = 1;
				M_assigned_time++;
				cpu_time++;

				for(i = 0; i < PR->size; i++)
				{
					PR->set[i].sequence--;
				}
				for(i = 0; i < SJF->size; i++)
				{
					SJF->set[i].sequence--;
				}


				if(task.service_location)
				{
					task.location = task.service_location;
					queue_push(&wait_SJF, task);
				}
				else
				{
				/*	printf("%s: complete = %d, arrive = %d, service = %d, waiting = %d\n",
							task.id,
							cpu_time,
							task.arrive_time,
							task.service_time,
							(cpu_time - task.arrive_time) - task.service_time);
*/
					sum_turnaround_time += cpu_time - task.arrive_time;
					sum_waiting_time += (cpu_time - task.arrive_time) - task.service_time;
					task.location = task.arrive_time;
					queue_push(&queue, task);

				}
			}
			select = 0;
		}
	}

	while(!check_empty_queue(FCFS))
	{
		task = queue_pop(FCFS);
		task.service_location--;
		task.execute[cpu_time] = 1;
		cpu_time++;

		if(task.service_location)
		{
			task.location = task.service_location;
			queue_push(FCFS, task);
		}
		else
		{
			/*printf("%s: complete = %d, arrive = %d, service = %d, waiting = %d\n",
					task.id,
					cpu_time,
					task.arrive_time,
					task.service_time,
					(cpu_time - task.arrive_time) - task.service_time);
			*/
			sum_turnaround_time += cpu_time - task.arrive_time;
			sum_waiting_time += (cpu_time - task.arrive_time) - task.service_time;
			task.location = task.arrive_time;
			queue_push(&queue, task);

		}
	}

	printf("\n[Multilevel Queue Scheduling]\n");
	while(!check_empty_queue(&queue))
	{
		task = queue_pop(&queue);
		printf("%s ",task.id);

		for(i = 0; i < cpu_time; i++)
		{
			putchar(task.execute[i] ? '*' : ' ');
		}
		printf("\n");
	}

	avg_turnaround_time = (float) sum_turnaround_time / (float) total_queue_size;
	avg_waiting_time = (float) sum_waiting_time / (float) total_queue_size;

	printf("\nCPU TIME: %d\n", cpu_time);
	printf("AVERAGE TURNAROUND TIME: %.2lf\n", avg_turnaround_time);
	printf("AVERAGE WAITING TIME: %.2lf\n", avg_waiting_time);
}

	int
main (int argc,	char **argv)
{
	frontier PR_Q;
	frontier SJF_Q;
	frontier FCFS_Q;

	PR_Q.size = 0;
	SJF_Q.size = 0;
	FCFS_Q.size = 0;

	if (argc <= 1)
	{
		MSG ("usage: %s input-file\n", argv[0]);
		return -1;
	}

	if (read_config (argv[1], &PR_Q, &SJF_Q, &FCFS_Q))
	{
		MSG ("failed to load config file '%s': %s\n", argv[1], STRERROR);
		return -1;
	}

	simulate(&PR_Q, &SJF_Q, &FCFS_Q);

	return 0;
}
