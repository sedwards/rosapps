#include "syshdrs.h"

#ifndef NO_SIGNALS
extern volatile Sjmp_buf gNetTimeoutJmp;
extern volatile Sjmp_buf gPipeJmp;
#endif

#ifndef NO_SIGNALS

int
SRecvfrom(int sfd, char *const buf, size_t size, int fl, struct sockaddr_in *const fromAddr, int tlen)
{
	int nread, tleft;
	vsio_sigproc_t sigalrm, sigpipe;
	time_t done, now;
	int alen;

	if (SSetjmp(gNetTimeoutJmp) != 0) {
		alarm(0);
		(void) SSignal(SIGALRM, (sio_sigproc_t) sigalrm);
		(void) SSignal(SIGPIPE, (sio_sigproc_t) sigpipe);
		errno = ETIMEDOUT;
		return (kTimeoutErr);
	}

	if (SSetjmp(gPipeJmp) != 0) {
		alarm(0);
		(void) SSignal(SIGALRM, (sio_sigproc_t) sigalrm);
		(void) SSignal(SIGPIPE, (sio_sigproc_t) sigpipe);
		errno = EPIPE;
		return (kBrokenPipeErr);
	}

	sigalrm = (vsio_sigproc_t) SSignal(SIGALRM, SIOHandler);
	sigpipe = (vsio_sigproc_t) SSignal(SIGPIPE, SIOHandler);

	time(&now);
	done = now + tlen;
	tleft = (int) (done - now);
	forever {
		alen = sizeof(struct sockaddr_in);
		(void) alarm((unsigned int) tleft);
		nread = recvfrom(sfd, buf, size, fl,
			(struct sockaddr *) fromAddr, &alen);
		(void) alarm(0);
		if (nread >= 0)
			break;
		if (errno != EINTR)
			break;		/* Fatal error. */
		errno = 0;
		time(&now);
		tleft = (int) (done - now);
		if (tleft < 1) {
			nread = kTimeoutErr;
			errno = ETIMEDOUT;
			break;
		}
	}

	(void) SSignal(SIGALRM, (sio_sigproc_t) sigalrm);
	(void) SSignal(SIGPIPE, (sio_sigproc_t) sigpipe);

	return (nread);
}	/* SRecvfrom */

#else

int
SRecvfrom(int sfd, char *const buf, size_t size, int fl, struct sockaddr_in *const fromAddr, int tlen)
{
	int nread, tleft;
	fd_set ss;
	struct timeval tv;
	int result;
	time_t done, now;
	int alen;

	time(&now);
	done = now + tlen;
	tleft = (int) (done - now);
	nread = 0;
	forever {
		alen = sizeof(struct sockaddr_in);
				
		forever {
			errno = 0;
			FD_ZERO(&ss);
			FD_SET(sfd, &ss);
			tv.tv_sec = tleft;
			tv.tv_usec = 0;
			result = select(sfd + 1, SELECT_TYPE_ARG234 &ss, NULL, NULL, SELECT_TYPE_ARG5 &tv);
			if (result == 1) {
				/* ready */
				break;
			} else if (result == 0) {
				/* timeout */
				errno = ETIMEDOUT;
				SETWSATIMEOUTERR
				return (kTimeoutErr);
			} else if (errno != EINTR) {
				return (-1);
			}
		}

		nread = recvfrom(sfd, buf, size, fl,
			(struct sockaddr *) fromAddr, &alen);

		if (nread >= 0)
			break;
		if (errno != EINTR)
			break;		/* Fatal error. */
		errno = 0;
		time(&now);
		tleft = (int) (done - now);
		if (tleft < 1) {
			nread = kTimeoutErr;
			errno = ETIMEDOUT;
			SETWSATIMEOUTERR
			break;
		}
	}

	return (nread);
}	/* SRecvfrom */

#endif

