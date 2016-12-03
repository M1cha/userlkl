#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <poll.h>
#include <lkl_host.h>
#include <lib/iomem.h>
#include <lib/jmp_buf.h>
#include <unistd.h>

#include <lk/kernel/semaphore.h>
#include <lk/kernel/mutex.h>
#include <lk/kernel/thread.h>

static void print(const char *str, int len)
{
	int ret __attribute__((unused));

	ret = write(STDOUT_FILENO, str, len);
}

struct lkl_mutex {
	int recursive;
	mutex_t mutex;
	semaphore_t sem;
};

struct lkl_sem {
	semaphore_t sem;
};

static struct lkl_sem *sem_alloc(int count)
{
	struct lkl_sem *sem;

	sem = malloc(sizeof(*sem));
	if (!sem)
		return NULL;

	sem_init(&sem->sem, count);

	return sem;
}

static void sem_free(struct lkl_sem *sem)
{
	sem_destroy(&sem->sem);
	free(sem);
}

static void sem_up(struct lkl_sem *sem)
{
	sem_post(&sem->sem, 1);
}

static void sem_down(struct lkl_sem *sem)
{
	int err;
	do {
		thread_yield();
		err = sem_wait(&sem->sem);
	} while (err < 0);
}

static struct lkl_mutex *mutex_alloc(int recursive)
{
	struct lkl_mutex *mutex = malloc(sizeof(struct lkl_mutex));

	if (!mutex)
		return NULL;

    recursive = 0;

	if (recursive)
		mutex_init(&mutex->mutex);
	else
		sem_init(&mutex->sem, 1);
	mutex->recursive = recursive;

	return mutex;
}

static void mutex_lock(struct lkl_mutex *mutex)
{
	int err;

	if (mutex->recursive)
		mutex_acquire(&mutex->mutex);
	else {
		do {
			thread_yield();
			err = sem_wait(&mutex->sem);
		} while (err < 0);
	}
}

static void mutex_unlock(struct lkl_mutex *mutex)
{
	if (mutex->recursive)
		mutex_release(&mutex->mutex);
	else {
		sem_post(&mutex->sem, 1);
	}
}

static void mutex_free(struct lkl_mutex *mutex)
{
	if (mutex->recursive)
		mutex_destroy(&mutex->mutex);
	else
		sem_destroy(&mutex->sem);
	free(mutex);
}

#define TIMER_INTERVAL 10
static volatile lk_time_t ticks = 0;

void TimerCallback(int sigNum) {
	ticks += 10;
    return;
	if (thread_timer_tick()==INT_RESCHEDULE) {
		thread_preempt();
	}
}
lk_time_t current_time(void)
{
	return ticks;
}

lk_bigtime_t current_time_hires(void)
{
	lk_bigtime_t time;

	time = (lk_bigtime_t)ticks * 1000;
	return time;
}

static void setup_scheduler(void) {
    struct sigaction schedulerHandle;
    struct itimerval timeQuantum;

    memset(&schedulerHandle, 0, sizeof (schedulerHandle));
    schedulerHandle.sa_handler = &TimerCallback;
    sigaction(SIGPROF, &schedulerHandle, NULL);

    timeQuantum.it_value.tv_sec = TIMER_INTERVAL/1000;
    timeQuantum.it_value.tv_usec = (TIMER_INTERVAL*1000) % 1000000;
    timeQuantum.it_interval = timeQuantum.it_value;
    setitimer(ITIMER_PROF, &timeQuantum, NULL);
}

void __attribute__ ((constructor)) lkl_thread_init(void)
{
	thread_init_early();
	thread_init();
	thread_create_idle();
	thread_set_priority(DEFAULT_PRIORITY);

    setup_scheduler();
}

static lkl_thread_t lkl_thread_create(void (*fn)(void *), void *arg)
{
	thread_t *thread = thread_create("lkl", (int (*)(void *))fn, arg, DEFAULT_PRIORITY, 2*1024*1024);
	if (!thread)
		return 0;
	else {
		thread_resume(thread);
		return (lkl_thread_t) thread;
	}
}

static void lkl_thread_detach(void)
{
	thread_detach(get_current_thread());
}

static void lkl_thread_exit(void)
{
	thread_exit(0);
}

static int lkl_thread_join(lkl_thread_t tid)
{
	if (thread_join((thread_t *)tid, NULL, INFINITE_TIME))
		return -1;
	else
		return 0;
}

static lkl_thread_t thread_self(void)
{
	return (lkl_thread_t)get_current_thread();
}

static int thread_equal(lkl_thread_t a, lkl_thread_t b)
{
	return a==b;
}

static unsigned long long time_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return 1e9*ts.tv_sec + ts.tv_nsec;
}

static void *timer_alloc(void (*fn)(void *), void *arg)
{
	int err;
	timer_t timer;
	struct sigevent se =  {
		.sigev_notify = SIGEV_THREAD,
		.sigev_value = {
			.sival_ptr = arg,
		},
		.sigev_notify_function = (void (*)(union sigval))fn,
	};

	err = timer_create(CLOCK_REALTIME, &se, &timer);
	if (err)
		return NULL;

	return (void *)(long)timer;
}

static int timer_set_oneshot(void *_timer, unsigned long ns)
{
	timer_t timer = (timer_t)(long)_timer;
	struct itimerspec ts = {
		.it_value = {
			.tv_sec = ns / 1000000000,
			.tv_nsec = ns % 1000000000,
		},
	};

	return timer_settime(timer, 0, &ts, NULL);
}

static void timer_free(void *_timer)
{
	timer_t timer = (timer_t)(long)_timer;

	timer_delete(timer);
}

static void lkl_panic(void)
{
	assert(0);
}

static long _gettid(void)
{
	return (long)get_current_thread();
}

static void* lkl_mem_alloc(unsigned long size) {
    return malloc(size);
}

struct lkl_host_operations lkl_host_ops = {
	.panic = lkl_panic,
	.thread_create = lkl_thread_create,
	.thread_detach = lkl_thread_detach,
	.thread_exit = lkl_thread_exit,
	.thread_join = lkl_thread_join,
	.thread_self = thread_self,
	.thread_equal = thread_equal,
	.sem_alloc = sem_alloc,
	.sem_free = sem_free,
	.sem_up = sem_up,
	.sem_down = sem_down,
	.mutex_alloc = mutex_alloc,
	.mutex_free = mutex_free,
	.mutex_lock = mutex_lock,
	.mutex_unlock = mutex_unlock,
	.time = time_ns,
	.timer_alloc = timer_alloc,
	.timer_set_oneshot = timer_set_oneshot,
	.timer_free = timer_free,
	.print = print,
	.mem_alloc = lkl_mem_alloc,
	.mem_free = free,
	.ioremap = lkl_ioremap,
	.iomem_access = lkl_iomem_access,
	.virtio_devices = lkl_virtio_devs,
	.gettid = _gettid,
	.jmp_buf_set = jmp_buf_set,
	.jmp_buf_longjmp = jmp_buf_longjmp,
};

static int fd_get_capacity(struct lkl_disk disk, unsigned long long *res)
{
	off_t off;

	off = lseek(disk.fd, 0, SEEK_END);
	if (off < 0)
		return -1;

	*res = off;
	return 0;
}

static int do_rw(ssize_t (*fn)(), struct lkl_disk disk, struct lkl_blk_req *req)
{
	off_t off = req->sector * 512;
	void *addr;
	int len;
	int i;
	int ret = 0;

	for (i = 0; i < req->count; i++) {

		addr = req->buf[i].iov_base;
		len = req->buf[i].iov_len;

		do {
			ret = fn(disk.fd, addr, len, off);

			if (ret <= 0) {
				ret = -1;
				goto out;
			}

			addr += ret;
			len -= ret;
			off += ret;

		} while (len);
	}

out:
	return ret;
}

static int blk_request(struct lkl_disk disk, struct lkl_blk_req *req)
{
	int err = 0;

	switch (req->type) {
	case LKL_DEV_BLK_TYPE_READ:
		err = do_rw(pread, disk, req);
		break;
	case LKL_DEV_BLK_TYPE_WRITE:
		err = do_rw(pwrite, disk, req);
		break;
	case LKL_DEV_BLK_TYPE_FLUSH:
	case LKL_DEV_BLK_TYPE_FLUSH_OUT:
#ifdef __linux__
		err = fdatasync(disk.fd);
#else
		err = fsync(disk.fd);
#endif
		break;
	default:
		return LKL_DEV_BLK_STATUS_UNSUP;
	}

	if (err < 0)
		return LKL_DEV_BLK_STATUS_IOERR;

	return LKL_DEV_BLK_STATUS_OK;
}

struct lkl_dev_blk_ops lkl_dev_blk_ops = {
	.get_capacity = fd_get_capacity,
	.request = blk_request,
};

