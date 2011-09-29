#ifndef MarksMemoryAnalyser_MutexSentry_h
#define MarksMemoryAnalyser_MutexSentry_h

namespace memcounter
{
	/** @brief Class to act as a thread mutex
	 *
	 * Automatically unlocks the thread when the function is left at any point, i.e. when
	 * the object is taken off the stack.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 29/Jul/2011
	 */
	class MutexSentry
	{
	public:
		MutexSentry( pthread_mutex_t& mutex ) : mutex_(mutex)
		{
			//std::cerr << "Locking thread..." << std::endl;
			pthread_mutex_lock(&mutex_);
		}
		~MutexSentry()
		{
			//std::cerr << "Unlocking thread..." << std::endl;
			pthread_mutex_unlock(&mutex_);
		}
	private:
		pthread_mutex_t& mutex_;
	};

} // end of the memcounter namespace
#endif
