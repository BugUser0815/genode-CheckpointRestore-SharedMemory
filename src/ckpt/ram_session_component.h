/*
 * \brief Intercepting Ram session
 * \author Denis Huber
 * \date 2016-08-12
 */

#ifndef _RTCR_RAM_SESSION_COMPONENT_H_
#define _RTCR_RAM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/log.h>
#include <base/rpc_server.h>
#include <ram_session/connection.h>

namespace Rtcr {
	class Ram_session_component;
}

class Rtcr::Ram_session_component : public Genode::Rpc_object<Genode::Ram_session>
{
private:
	static constexpr bool verbose = false;

	Genode::Entrypoint &_ep;
	Genode::Allocator  &_md_alloc;

	/**
	 * Connection to the parent Ram session (usually core's Ram session)
	 */
	Genode::Ram_connection _parent_ram;

public:

	Ram_session_component(Genode::Entrypoint &ep, Genode::Allocator &md_alloc)
	:
		_ep(ep), _md_alloc(md_alloc), _parent_ram()
	{
		_ep.manage(*this);
		if(verbose) Genode::log("Ram_session_component created");
	}

	~Ram_session_component()
	{
		_ep.dissolve(*this);
		if(verbose) Genode::log("Ram_session_component destroyed");
	}

	Genode::Ram_session_capability parent_ram_cap()
	{
		return _parent_ram.cap();
	}

	/***************************
	 ** Ram_session interface **
	 ***************************/

	Genode::Ram_dataspace_capability alloc(Genode::size_t size, Genode::Cache_attribute cached) override
	{
		if(verbose) Genode::log("Ram::alloc()");
		return _parent_ram.alloc(size, cached);
	}

	void free(Genode::Ram_dataspace_capability ds) override
	{
		if(verbose) Genode::log("Ram::free()");
		_parent_ram.free(ds);
	}

	int ref_account(Genode::Ram_session_capability ram_session) override
	{
		if(verbose) Genode::log("Ram::ref_account()");
		return _parent_ram.ref_account(ram_session);
	}

	int transfer_quota(Genode::Ram_session_capability ram_session, Genode::size_t amount) override
	{
		if(verbose) Genode::log("Ram::transfer_quota()");
		return _parent_ram.transfer_quota(ram_session, amount);
	}

	Genode::size_t quota() override
	{
		if(verbose) Genode::log("Ram::quota()");
		return _parent_ram.quota();
	}

	Genode::size_t used() override
	{
		if(verbose) Genode::log("Ram::used()");
		return _parent_ram.used();
	}

};

#endif /* _RTCR_RAM_SESSION_COMPONENT_H_ */
