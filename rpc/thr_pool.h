#ifndef __THR_POOL__
#define __THR_POOL__

#include <pthread.h>
#include <vector>

#include "fifo.h"

class ThrPool {


	public:
		struct job_t {
			void *(*f)(void *); //function point
			void *a; //function arguments
		};

		ThrPool(int sz, bool blocking=true);
		~ThrPool();
		template<class C, class A> bool addObjJob(C *o, void (C::*m)(A), A a);
		template<class C, class A0, class A1> bool addObjJob(C *o, void (C::*m)(A0, A1), A0 a0, A1 a1);
		template<class C, class A0, class A1, class A2> bool addObjJob(C *o, void (C::*m)(A0, A1, A2), A0 a0, A1 a1, A2 a2);
		template<class C, class A0, class A1, class A2, class A3> bool addObjJob(C *o, void (C::*m)(A0, A1, A2, A3), A0 a0, A1 a1, A2 a2, A3 a3);

		bool takeJob(job_t *j);

		void destroy();
	private:
		pthread_attr_t attr_;
		int nthreads_;
		bool blockadd_;
		bool stopped;

		fifo<job_t> jobq_;
		std::vector<pthread_t> th_;

		bool addJob(void *(*f)(void *), void *a);
};

	template <class C, class A> bool 
ThrPool::addObjJob(C *o, void (C::*m)(A), A a)
{
	class objfunc_wrapper {
		public:
			C *o;
			void (C::*m)(A a);
			A a;
			static void *func(void *vvv) {
				objfunc_wrapper *x = (objfunc_wrapper*)vvv;
				C *o = x->o;
				void (C::*m)(A ) = x->m;
				A a = x->a;
				(o->*m)(a);
				delete x;
				return 0;
			}
	};

	objfunc_wrapper *x = new objfunc_wrapper;
	x->o = o;
	x->m = m;
	x->a = a;
	return addJob(&objfunc_wrapper::func, (void *)x);
}

	template<class C, class A0, class A1> bool 
ThrPool::addObjJob(C *o, void (C::*m)(A0, A1), A0 a0, A1 a1) 
{
	class objfunc_wrapper {
		public:
			C *o;
			void (C::*m)(A0 a0, A1 a1);
			A0 a0;
			A1 a1;
			static void *func(void *vvv) {
				objfunc_wrapper *x = (objfunc_wrapper*)vvv;
				C *o = x->o;
				void (C::*m)(A0 a0, A1 a1) = x->m;
				A0 a0 = x->a0;
				A1 a1 = x->a1;
				(o->*m)(a0, a1);
				delete x;
				return 0;
			}
	};

	objfunc_wrapper *x = new objfunc_wrapper;
	x->o = o;
	x->m = m;
	x->a0 = a0;
	x->a1 = a1;
	return addJob(&objfunc_wrapper::func, (void *)x);
}

	template<class C, class A0, class A1, class A2> bool 
ThrPool::addObjJob(C *o, void (C::*m)(A0, A1, A2), A0 a0, A1 a1, A2 a2) 
{
	class objfunc_wrapper {
		public:
			C *o;
			void (C::*m)(A0 a0, A1 a1, A2 a2);
			A0 a0;
			A1 a1;
			A2 a2;
			static void *func(void *vvv) {
				objfunc_wrapper *x = (objfunc_wrapper*)vvv;
				C *o = x->o;
				void (C::*m)(A0 a0, A1 a1, A2 a2) = x->m;
				A0 a0 = x->a0;
				A1 a1 = x->a1;
				A2 a2 = x->a2;
				(o->*m)(a0, a1, a2);
				delete x;
				return 0;
			}
	};

	objfunc_wrapper *x = new objfunc_wrapper;
	x->o = o;
	x->m = m;
	x->a0 = a0;
	x->a1 = a1;
	x->a2 = a2;
	return addJob(&objfunc_wrapper::func, (void *)x);
}

template<class C, class A0, class A1, class A2, class A3> bool 
ThrPool::addObjJob(C *o, void (C::*m)(A0, A1, A2, A3), A0 a0, A1 a1, A2 a2, A3 a3) 
{
	class objfunc_wrapper {
		public:
			C *o;
			void (C::*m)(A0 a0, A1 a1, A2 a2, A3 a3);
			A0 a0;
			A1 a1;
			A2 a2;
			A3 a3;
			static void *func(void *vvv) {
				objfunc_wrapper *x = (objfunc_wrapper*)vvv;
				C *o = x->o;
				void (C::*m)(A0 a0, A1 a1, A2 a2, A3 a3) = x->m;
				A0 a0 = x->a0;
				A1 a1 = x->a1;
				A2 a2 = x->a2;
				A3 a3 = x->a3;
				(o->*m)(a0, a1, a2, a3);
				delete x;
				return 0;
			}
	};

	objfunc_wrapper *x = new objfunc_wrapper;
	x->o = o;
	x->m = m;
	x->a0 = a0;
	x->a1 = a1;
	x->a2 = a2;
	x->a3 = a3;
	return addJob(&objfunc_wrapper::func, (void *)x);
}

#endif

