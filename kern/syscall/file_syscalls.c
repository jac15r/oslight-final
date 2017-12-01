/*
 * File-related system call implementations.
 */

//#include <stdlib.h>
#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/seek.h>
#include <kern/stat.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <copyinout.h>
#include <vfs.h>
#include <vnode.h>
#include <openfile.h>
#include <filetable.h>
#include <syscall.h>

/*
 * open() - get the path with copyinstr, then use openfile_open and
 * filetable_place to do the real work.
 */
int
sys_open(const_userptr_t filename, int flags, mode_t mode, int *retval)
{
	const int allflags = O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC | O_APPEND | O_NOCTTY;

	char *kpath;
	struct openfile *file;
	int result = 0;

	/* 
	 * Your implementation of system call open starts here.  
	 *
	 * Check the design document design/filesyscall.txt for the steps
	 */
	//(void) upath; // suppress compilation warning until code gets written
	//(void) flags; // suppress compilation warning until code gets written
	//(void) mode; // suppress compilation warning until code gets written
	//(void) retval; // suppress compilation warning until code gets written
	//(void) allflags; // suppress compilation warning until code gets written
	//(void) kpath; // suppress compilation warning until code gets written
	//(void) file; // suppress compilation warning until code gets written

	/*result = copyinstr(filename, kpath, __PATH_MAX, NULL);
	if (result) {
		kfree(kpath);
		return result;
	}

	kfree(kpath);
	*/

	if(filename == NULL){
		*retval = EFAULT;
		return -1;
	}

	int flagcheck = flags & allflags;
	if(flagcheck != O_RDONLY && flagcheck != O_WRONLY && flagcheck != O_RDWR){
		*retval = EINVAL;
		return -1;
	}

	//the filename goes into kpath for openfile_open
	if((kpath = (char *)kmalloc(__PATH_MAX)) == NULL)
		return ENOMEM;

	result = copyinstr(filename, kpath, __PATH_MAX, NULL);
	if(result){
		kfree(kpath);
		return result;
	}

	result = openfile_open(kpath,flags, mode, &file);
	if(result){
		kfree(kpath);
		*retval = result;
		return -1;
	}

	//int *fd_ret = NULL;
	filetable_place(curproc->p_filetable, file, &result);
	kfree(kpath);
	return result;
}
//struct iovec;
//struct uio;
//initialize the uio struct for use
/*void make_uio(struct iovec *iov, struct uio *u, userptr_t buf, size_t size, off_t offset{

	iov->iov_ubase = buf;
	iov->iov_len = size;
	u->uio_iov = iov;
	u->uio_iovcnt = 1;
	u->uio_offset = offset;
	u->uio_resid = size;

	u->uio_segflg = UIO_USERSPACE;
	u->uio_rw = UIO_READ;
	u->uio_space = curproc->p_addrspace;

}*/

/*
 * read() - read data from a file
 */
int
sys_read(int fd, userptr_t buf, size_t size, int *retval)
{
       int result = 0;
       struct openfile *open;

       /* 
        * Your implementation of system call read starts here.  
        *
        * Check the design document design/filesyscall.txt for the steps
        */
       //(void) fd; // suppress compilation warning until code gets written
       //(void) buf; // suppress compilation warning until code gets written
       //(void) size; // suppress compilation warning until code gets written
       //(void) retval; // suppress compilation warning until code gets written

	result = filetable_get(curproc->p_filetable, fd, &open);
	if(result)
		return EINVAL;

	//of_offsetlock or of_reflock?
	lock_acquire(curproc->p_filetable->ft_openfiles[fd]->of_offsetlock);

	if(open->of_accmode == O_WRONLY){
		lock_release(curproc->p_filetable->ft_openfiles[fd]->of_offsetlock);
		return EBADF;
	}

	//use uio here
	//make_uio(user_iov, user_uio, buf, size, curproc->p_filetable[fd]->of_offset/*, UIO_READ*/);

	struct uio *user_uio = kmalloc(sizeof(*user_uio));
	struct iovec *user_iov = kmalloc(sizeof(*user_iov));
	
	user_iov->iov_ubase = buf;
	user_iov->iov_len = size;
	user_uio->uio_iov = user_iov;
	user_uio->uio_iovcnt = 1;
	user_uio->uio_offset = curproc->p_filetable->ft_openfiles[fd]->of_offset;
	user_uio->uio_resid = size;

	user_uio->uio_segflg = UIO_USERSPACE;
	user_uio->uio_rw = /*rw*/UIO_READ;
	user_uio->uio_space = curproc->p_addrspace;

	if(buf == NULL || curproc->p_filetable->ft_openfiles[fd]->of_vnode == NULL){
		lock_release(curproc->p_filetable->ft_openfiles[fd]->of_offsetlock);
		kfree(user_uio);
		kfree(user_iov);
		return EFAULT;
	}

	result = VOP_READ(curproc->p_filetable->ft_openfiles[fd]->of_vnode, user_uio);
	if(result){
		lock_release(curproc->p_filetable->ft_openfiles[fd]->of_offsetlock);
		kfree(user_uio);
		kfree(user_iov);
		return result;
	}

	*retval = size - user_uio->uio_resid;

	lock_release(curproc->p_filetable->ft_openfiles[fd]->of_offsetlock);

	kfree(user_uio);
	kfree(user_iov);

	result = 0;
       return result;
}

/*
 * write() - write data to a file
 */

int
sys_write(int fd, userptr_t buf, size_t size, int *retval)
{
       int result = 0;
       struct openfile *open;

	result = filetable_get(curproc->p_filetable, fd, &open);
	if(result)
		return result;

	lock_acquire(curproc->p_filetable->ft_openfiles[fd]->of_offsetlock);

	if(open->of_accmode == O_RDONLY){
		lock_release(curproc->p_filetable->ft_openfiles[fd]->of_offsetlock);
		return EBADF;
	}

	struct uio *user_uio = kmalloc(sizeof(*user_uio));
	struct iovec *user_iov = kmalloc(sizeof(*user_iov));
	
	user_iov->iov_ubase = buf;
	user_iov->iov_len = size;
	user_uio->uio_iov = user_iov;
	user_uio->uio_iovcnt = 1;
	user_uio->uio_offset = curproc->p_filetable->ft_openfiles[fd]->of_offset;
	user_uio->uio_resid = size;

	user_uio->uio_segflg = UIO_USERSPACE;
	user_uio->uio_rw = UIO_WRITE;
	user_uio->uio_space = curproc->p_addrspace;

	if(buf == NULL || curproc->p_filetable->ft_openfiles[fd]->of_vnode == NULL){
		lock_release(curproc->p_filetable->ft_openfiles[fd]->of_offsetlock);
		kfree(user_uio);
		kfree(user_iov);
		return EFAULT;
	}

	result = VOP_WRITE(curproc->p_filetable->ft_openfiles[fd]->of_vnode, user_uio);
	if(result){
		lock_release(curproc->p_filetable->ft_openfiles[fd]->of_offsetlock);
		kfree(user_uio);
		kfree(user_iov);
		return result;
	}

	*retval = size - user_uio->uio_resid;

	lock_release(curproc->p_filetable->ft_openfiles[fd]->of_offsetlock);

	kfree(user_uio);
	kfree(user_iov);

	result = 0;
       return result;
}
/*
 * close() - remove from the file table.
 */

int sys_close(int fd){

	int ret = 0;
	struct openfile *nullfile = NULL;
	struct openfile *oldfile = NULL;

	if(!filetable_okfd(curproc->p_filetable, fd))
		return EBADF;
	//if(curproc->p_filetable[fd] == NULL)
	//	return EBADF;

	filetable_placeat(curproc->p_filetable, nullfile, fd, &oldfile);

	if(oldfile == NULL)
		return EBADF;

	return ret;

}

/* 
* meld () - combine the content of two files word by word into a new file
*/

//since realloc and krealloc don't exist, I wrote a basic equivalent
int realloc(void *ptr, size_t new_size){
	void **newptr;
	newptr = kmalloc(sizeof(new_size));

	if(newptr == NULL){
		return ENOMEM;
	}
	memcpy(newptr, ptr, sizeof(*ptr));
	kfree(ptr);
	ptr = newptr;
	newptr = NULL;

	return 0;
}

//more stdlib stuff
char *
strncpy(char *s1, const char *s2, size_t n)
{
	char *s = s1;
	while (n > 0 && *s2 != '\0') {
		*s++ = *s2++;
		--n;
	}
	while (n > 0) {
		*s++ = '\0';
		--n;
	}
	return s1;
}

/*int sys_meld(const_userptr_t filename1, int filehandle1, void *buf1, size_t size1,
		const_userptr_t filename2, int filehandle2, void *buf2, size_t size2,
		const_userptr_t filename3, int filehandle3, void *buf3, size_t size3){
*/
int sys_meld(struct meldargs * meldarg1, struct meldargs * meldarg2, struct meldargs * meldarg3){
	int result = 0;
	struct openfile *open1;
	struct openfile *open2;
	struct openfile *open3;

	char *kpath1;
	char *kpath2;
	char *kpath3;

	//Extract arguments
	const_userptr_t filename1 = meldarg1->filename;
	const_userptr_t filename2 = meldarg2->filename;
	const_userptr_t filename3 = meldarg3->filename;

	int filehandle1 = meldarg1->filehandle;
	int filehandle2 = meldarg2->filehandle;
	int filehandle3 = meldarg3->filehandle;

	void *buf1 = meldarg1->buf;
	void *buf2 = meldarg2->buf;
	void *buf3 = meldarg3->buf;

	size_t size1 = meldarg1->size;
	size_t size2 = meldarg2->size;
	size_t size3 = meldarg3->size;

	//copy names
	if((kpath1 = (char *)kmalloc(__PATH_MAX)) == NULL)
		return ENOMEM;

	if((kpath2 = (char *)kmalloc(__PATH_MAX)) == NULL)
		return ENOMEM;
	
	if((kpath3 = (char *)kmalloc(__PATH_MAX)) == NULL)
		return ENOMEM;

	result = copyinstr(filename1, kpath1, __PATH_MAX, NULL);
	if(result){
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		return result;
	}

	result = copyinstr(filename2, kpath2, __PATH_MAX, NULL);
	if(result){
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		return result;
	}

	result = copyinstr(filename3, kpath3, __PATH_MAX, NULL);
	if(result){
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		return result;
	}

	//open files
	result = openfile_open(kpath1,O_RDONLY, O_RDONLY, &open1);
	if(result){
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		return result;
	}

	result = openfile_open(kpath2,O_RDONLY, O_RDONLY, &open2);
	if(result){
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		return result;
	}

	result = openfile_open(kpath3,O_WRONLY, O_WRONLY, &open3);
	if(result){
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		return result;
	}

	//Check for existence of file 1 and 2
	/*result = filetable_get(curproc->p_filetable, filehandle1, &open1);
	if(result){
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		return EINVAL;
	}
		result = filetable_get(curproc->p_filetable, filehandle2, &open2);
	if(result){
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		return EINVAL;
	}*/

	//Check for the nonexistence of file 3
	result = filetable_get(curproc->p_filetable, filehandle3, &open3);
	if(!result){
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		return EINVAL;
	}

	//place in filetable
	result = filetable_place(curproc->p_filetable, open1, &result);
	if(result){
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		return EINVAL;
	}

	result = filetable_place(curproc->p_filetable, open2, &result);
	if(result){
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		return EINVAL;
	}

	result = filetable_place(curproc->p_filetable, open3, &result);
	if(result){
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		return EINVAL;
	}

	//lock time
	lock_acquire(curproc->p_filetable->ft_openfiles[filehandle1]->of_offsetlock);
	lock_acquire(curproc->p_filetable->ft_openfiles[filehandle2]->of_offsetlock);
	lock_acquire(curproc->p_filetable->ft_openfiles[filehandle3]->of_offsetlock);

	//check for correct flags
	/*if(open1->of_accmode == O_WRONLY){
		lock_release(curproc->p_filetable->ft_openfiles[filehandle1]->of_offsetlock);
		lock_release(curproc->p_filetable->ft_openfiles[filehandle2]->of_offsetlock);
		lock_release(curproc->p_filetable->ft_openfiles[filehandle3]->of_offsetlock);
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		return EBADF;
	}*/
	

	//Create uio and iovec structs to move data over
	struct uio *user_uio1 = kmalloc(sizeof(*user_uio1));
	struct iovec *user_iov1 = kmalloc(sizeof(*user_iov1));
	
	struct uio *user_uio2 = kmalloc(sizeof(*user_uio2));
	struct iovec *user_iov2 = kmalloc(sizeof(*user_iov2));
	
	struct uio *user_uio3 = kmalloc(sizeof(*user_uio3));
	struct iovec *user_iov3 = kmalloc(sizeof(*user_iov3));

	user_iov1->iov_ubase = buf1;
	user_iov1->iov_len = size1;
	user_uio1->uio_iov = user_iov1;
	user_uio1->uio_iovcnt = 1;
	user_uio1->uio_offset = curproc->p_filetable->ft_openfiles[filehandle1]->of_offset;
	user_uio1->uio_resid = size1;
	user_uio1->uio_segflg = UIO_USERSPACE;
	user_uio1->uio_rw = UIO_READ;
	user_uio1->uio_space = curproc->p_addrspace;

	user_iov2->iov_ubase = buf2;
	user_iov2->iov_len = size2;
	user_uio2->uio_iov = user_iov2;
	user_uio2->uio_iovcnt = 1;
	user_uio2->uio_offset = curproc->p_filetable->ft_openfiles[filehandle2]->of_offset;
	user_uio2->uio_resid = size2;
	user_uio2->uio_segflg = UIO_USERSPACE;
	user_uio2->uio_rw = UIO_READ;
	user_uio2->uio_space = curproc->p_addrspace;

	user_iov3->iov_ubase = buf3;
	user_iov3->iov_len = size1 + size2;
	user_uio3->uio_iov = user_iov3;
	user_uio3->uio_iovcnt = 1;
	user_uio3->uio_offset = curproc->p_filetable->ft_openfiles[filehandle3]->of_offset;
	user_uio3->uio_resid = size3;
	user_uio3->uio_segflg = UIO_USERSPACE;
	user_uio3->uio_rw = UIO_WRITE;
	user_uio3->uio_space = curproc->p_addrspace;

	//read in
	result = VOP_READ(curproc->p_filetable->ft_openfiles[filehandle1]->of_vnode, user_uio1);
	if(result){
		lock_release(curproc->p_filetable->ft_openfiles[filehandle1]->of_offsetlock);
		lock_release(curproc->p_filetable->ft_openfiles[filehandle2]->of_offsetlock);
		lock_release(curproc->p_filetable->ft_openfiles[filehandle3]->of_offsetlock);
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		kfree(user_uio1);
		kfree(user_uio2);
		kfree(user_uio3);
		//kfree(user_iov1);
		//kfree(user_iov2);
		//kfree(user_iov3);
		return result;
	}

	result = VOP_READ(curproc->p_filetable->ft_openfiles[filehandle2]->of_vnode, user_uio2);
	if(result){
		lock_release(curproc->p_filetable->ft_openfiles[filehandle1]->of_offsetlock);
		lock_release(curproc->p_filetable->ft_openfiles[filehandle2]->of_offsetlock);
		lock_release(curproc->p_filetable->ft_openfiles[filehandle3]->of_offsetlock);
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		kfree(user_uio1);
		kfree(user_uio2);
		kfree(user_uio3);
		//kfree(user_iov1);
		//kfree(user_iov2);
		//kfree(user_iov3);
		return result;
	}

	int eithersize = 0;
	int i = 0;
	unsigned int markertotal = 0;
	unsigned int marker1 = 0;
	unsigned int marker2 = 0;

	//pad
	if(user_uio1->uio_iov->iov_len % 4 == 1){
		user_uio1->uio_iov->iov_len += 3;
		result = realloc(user_uio1->uio_iov->iov_ubase, user_uio1->uio_iov->iov_len + 3);
		if(result){
			lock_release(curproc->p_filetable->ft_openfiles[filehandle1]->of_offsetlock);
			lock_release(curproc->p_filetable->ft_openfiles[filehandle2]->of_offsetlock);
			lock_release(curproc->p_filetable->ft_openfiles[filehandle3]->of_offsetlock);
			kfree(kpath1);
			kfree(kpath2);
			kfree(kpath3);
			kfree(user_uio1);
			kfree(user_uio2);
			kfree(user_uio3);
			return ENOMEM;
		}
		strncpy((char *)(user_uio1->uio_iov->iov_ubase + user_uio1->uio_iov->iov_len - 3), "   ", 3);
		realloc(user_uio3->uio_iov->iov_ubase,user_uio3->uio_iov->iov_len + 3);
		user_uio3->uio_iov->iov_len += 3;
	}

	if(user_uio1->uio_iov->iov_len % 4 == 2){
		user_uio1->uio_iov->iov_len += 2;
		result = realloc(user_uio1->uio_iov->iov_ubase, user_uio1->uio_iov->iov_len + 2);
		if(result){
			lock_release(curproc->p_filetable->ft_openfiles[filehandle1]->of_offsetlock);
			lock_release(curproc->p_filetable->ft_openfiles[filehandle2]->of_offsetlock);
			lock_release(curproc->p_filetable->ft_openfiles[filehandle3]->of_offsetlock);
			kfree(kpath1);
			kfree(kpath2);
			kfree(kpath3);
			kfree(user_uio1);
			kfree(user_uio2);
			kfree(user_uio3);
			return ENOMEM;
		}
		strncpy((char *)(user_uio1->uio_iov->iov_ubase + user_uio1->uio_iov->iov_len - 2), "   ", 2);
		realloc(user_uio3->uio_iov->iov_ubase,user_uio3->uio_iov->iov_len + 2);
		user_uio3->uio_iov->iov_len += 2;
	}

	if(user_uio1->uio_iov->iov_len % 4 == 3){
		user_uio1->uio_iov->iov_len += 1;
		result = realloc(user_uio1->uio_iov->iov_ubase, user_uio1->uio_iov->iov_len + 1);
		if(result){
			lock_release(curproc->p_filetable->ft_openfiles[filehandle1]->of_offsetlock);
			lock_release(curproc->p_filetable->ft_openfiles[filehandle2]->of_offsetlock);
			lock_release(curproc->p_filetable->ft_openfiles[filehandle3]->of_offsetlock);
			kfree(kpath1);
			kfree(kpath2);
			kfree(kpath3);
			kfree(user_uio1);
			kfree(user_uio2);
			kfree(user_uio3);
			return ENOMEM;
		}
		strncpy((char *)(user_uio1->uio_iov->iov_ubase + user_uio1->uio_iov->iov_len - 1), "   ", 1);
		realloc(user_uio3->uio_iov->iov_ubase,user_uio3->uio_iov->iov_len + 1);
		user_uio3->uio_iov->iov_len += 1;
	}

	if(user_uio2->uio_iov->iov_len % 4 == 1){
		user_uio2->uio_iov->iov_len += 3;
		result = realloc(user_uio2->uio_iov->iov_ubase, user_uio2->uio_iov->iov_len + 3);
		if(result){
			lock_release(curproc->p_filetable->ft_openfiles[filehandle1]->of_offsetlock);
			lock_release(curproc->p_filetable->ft_openfiles[filehandle2]->of_offsetlock);
			lock_release(curproc->p_filetable->ft_openfiles[filehandle3]->of_offsetlock);
			kfree(kpath1);
			kfree(kpath2);
			kfree(kpath3);
			kfree(user_uio1);
			kfree(user_uio2);
			kfree(user_uio3);
			return ENOMEM;
		}
		strncpy((char *)(user_uio2->uio_iov->iov_ubase + user_uio2->uio_iov->iov_len - 3), "   ", 3);
		realloc(user_uio3->uio_iov->iov_ubase,user_uio3->uio_iov->iov_len + 3);
		user_uio3->uio_iov->iov_len += 3;
	}

	if(user_uio2->uio_iov->iov_len % 4 == 2){
		user_uio2->uio_iov->iov_len += 2;
		result = realloc(user_uio2->uio_iov->iov_ubase, user_uio2->uio_iov->iov_len + 2);
		if(result){
			lock_release(curproc->p_filetable->ft_openfiles[filehandle1]->of_offsetlock);
			lock_release(curproc->p_filetable->ft_openfiles[filehandle2]->of_offsetlock);
			lock_release(curproc->p_filetable->ft_openfiles[filehandle3]->of_offsetlock);
			kfree(kpath1);
			kfree(kpath2);
			kfree(kpath3);
			kfree(user_uio1);
			kfree(user_uio2);
			kfree(user_uio3);
			return ENOMEM;
		}
		strncpy((char *)(user_uio2->uio_iov->iov_ubase + user_uio2->uio_iov->iov_len - 2), "   ", 2);
		realloc(user_uio3->uio_iov->iov_ubase,user_uio3->uio_iov->iov_len + 2);
		user_uio3->uio_iov->iov_len += 2;
	}

	if(user_uio2->uio_iov->iov_len % 4 == 3){
		user_uio2->uio_iov->iov_len += 1;
		result = realloc(user_uio2->uio_iov->iov_ubase, user_uio2->uio_iov->iov_len + 1);
		if(result){
			lock_release(curproc->p_filetable->ft_openfiles[filehandle1]->of_offsetlock);
			lock_release(curproc->p_filetable->ft_openfiles[filehandle2]->of_offsetlock);
			lock_release(curproc->p_filetable->ft_openfiles[filehandle3]->of_offsetlock);
			kfree(kpath1);
			kfree(kpath2);
			kfree(kpath3);
			kfree(user_uio1);
			kfree(user_uio2);
			kfree(user_uio3);
			return ENOMEM;
		}
		strncpy((char *)(user_uio2->uio_iov->iov_ubase + user_uio2->uio_iov->iov_len - 1), "   ", 1);
		realloc(user_uio3->uio_iov->iov_ubase,user_uio3->uio_iov->iov_len + 1);
		user_uio3->uio_iov->iov_len += 1;
	}

	//write in
	while(eithersize == 0){

		strncpy((char *)(user_uio3->uio_iov->iov_ubase+markertotal), (char *)(user_iov1->iov_ubase+marker1), 4);
		markertotal += 4;
		marker1 += 4;
		strncpy((char *)(user_uio3->uio_iov->iov_ubase+markertotal), (char *)(user_iov2->iov_ubase+marker2), 4);
		markertotal += 4;
		marker2 += 4;
		i++;

		if(marker1 == user_uio1->uio_iov->iov_len && marker2 == user_uio2->uio_iov->iov_len)
			eithersize = 3;

		if(marker1 == user_uio1->uio_iov->iov_len)
			eithersize = 1;
	
		if(marker2 == user_uio2->uio_iov->iov_len)
			eithersize = 2;
	}

	//finish writing in
	if(eithersize == 1 ){
		strncpy((char *)(user_uio3->uio_iov->iov_ubase+markertotal), (char *)(user_iov2->iov_ubase+marker2), user_uio2->uio_iov->iov_len-marker2);
	}

	if(eithersize == 2 ){
		strncpy((char *)(user_uio3->uio_iov->iov_ubase+markertotal), (char *)(user_iov1->iov_ubase+marker1), user_uio1->uio_iov->iov_len-marker1);
	}

	result = VOP_WRITE(curproc->p_filetable->ft_openfiles[filehandle3]->of_vnode, user_uio3);
	if(result){
		lock_release(curproc->p_filetable->ft_openfiles[filehandle1]->of_offsetlock);
		lock_release(curproc->p_filetable->ft_openfiles[filehandle2]->of_offsetlock);
		lock_release(curproc->p_filetable->ft_openfiles[filehandle3]->of_offsetlock);
		kfree(kpath1);
		kfree(kpath2);
		kfree(kpath3);
		kfree(user_uio1);
		kfree(user_uio2);
		kfree(user_uio3);
		return result;
	}

	sys_close(filehandle1);
	sys_close(filehandle2);
	sys_close(filehandle3);

	lock_release(curproc->p_filetable->ft_openfiles[filehandle1]->of_offsetlock);
	lock_release(curproc->p_filetable->ft_openfiles[filehandle2]->of_offsetlock);
	lock_release(curproc->p_filetable->ft_openfiles[filehandle3]->of_offsetlock);
	kfree(kpath1);
	kfree(kpath2);
	kfree(kpath3);
	kfree(user_uio1);
	kfree(user_uio2);
	kfree(user_uio3);
	//kfree(user_iov1);
	//kfree(user_iov2);
	//kfree(user_iov3);
	result = 0;
	return result;
}
