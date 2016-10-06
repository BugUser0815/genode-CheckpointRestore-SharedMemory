/*
 * \brief  Intercepting Rm session
 * \author Denis Huber
 * \date   2016-10-02
 */

#ifndef _RTCR_RM_SESSION_H_
#define _RTCR_RM_SESSION_H_

/* Genode includes */
#include <rm_session/rm_session.h>
#include <root/component.h>
#include <util/list.h>

/* Rtcr includes */
#include "region_map_component.h"

namespace Rtcr {
	struct Region_map_info;
	struct Rm_session_info;
	class Rm_session_component;
	class Rm_root;

	constexpr bool rm_verbose_debug = false;
	constexpr bool rm_root_verbose_debug = false;
}

/**
 * List element for managing Region_map_components created through an Rm_session
 *
 * Provides information about Region_map capability, and attached regions
 */
struct Rtcr::Region_map_info : Genode::List<Region_map_info>::Element
{
	/**
	 * Reference to session object; encapsulates capability and object's state
	 */
	Region_map_component &region_map;

	Region_map_info(Region_map_component &region_map)
	:
		region_map(region_map)
	{ }

	Region_map_info *find_by_cap(Genode::Capability<Genode::Region_map> cap)
	{
		if(cap == region_map.cap())
			return this;
		Region_map_info *rm_info = next();
		return rm_info ? rm_info->find_by_cap(cap) : 0;
	}
};

/**
 * List element for managing Rm_session_components
 */
struct Rtcr::Rm_session_info : Genode::List<Rm_session_info>::Element
{
	/**
	 * Reference to the session object; encapsulates capability and object's state
	 */
	Rm_session_component &rms;
	/**
	 * Arguments provided for creating the session object
	 */
	const char           *args;

	Rm_session_info(Rm_session_component &rms, const char* args)
	:
		rms  (rms),
		args (args)
	{ }

	Rm_session_info *find_by_ptr(Rm_session_component *ptr)
	{
		if(ptr == &rms)
			return this;
		Rm_session_info *rms_info = next();
		return rms_info ? rms_info->find_by_ptr(ptr) : 0;
	}

};

class Rtcr::Rm_session_component : public Genode::Rpc_object<Genode::Rm_session>
{
private:
	/**
	 * Enable log output for debugging
	 */
	static constexpr bool verbose_debug = rm_verbose_debug;

	/**
	 * Allocator for Rpc objects created by this session and also for monitoring structures
	 */
	Genode::Allocator             &_md_alloc;
	/**
	 * Entrypoint for managing created Rpc objects
	 */
	Genode::Entrypoint            &_ep;
	/**
	 * Parent's session connection which is used by the intercepted methods
	 */
	Genode::Rm_connection          _parent_rm;
    /**
     * Lock for infos list
     */
	Genode::Lock                   _infos_lock;
    /**
     * List for monitoring Rpc object
     */
	Genode::List<Region_map_info>  _region_map_infos;

public:
	Rm_session_component(Genode::Env &env, Genode::Allocator &md_alloc, Genode::Entrypoint &ep)
	:
		_md_alloc         (md_alloc),
		_ep               (ep),
		_parent_rm        (env),
		_infos_lock       (),
		_region_map_infos ()
	{
		if(verbose_debug) Genode::log("\033[33m", __func__, "\033[0m");
	}

	~Rm_session_component()
	{
		// Destroy all list elements through destroy method
		Region_map_info *rms_info = nullptr;
		while((rms_info = _region_map_infos.first()))
		{
			_region_map_infos.remove(rms_info);
			destroy(rms_info->region_map.cap());
		}

		if(verbose_debug) Genode::log("\033[33m", __func__, "\033[0m");
	}

	Genode::List<Region_map_info> &region_map_infos() { return _region_map_infos; }
    void region_map_infos(Genode::List<Region_map_info> &infos) { _region_map_infos = infos; }

	/******************************
	 ** Rm session Rpc interface **
	 ******************************/

	/**
	 * Create a virtual Region map, its real counter part and a list element to manage them
	 */
	Genode::Capability<Genode::Region_map> create(Genode::size_t size)
	{
		if(verbose_debug) Genode::log("Rm::\033[33m", __func__, "\033[0m(size=", size, ")");

		// Create real Region_map from parent
		auto parent_cap = _parent_rm.create(size);

		// Create virtual Region_map
		Region_map_component *new_region_map =
				new (_md_alloc) Region_map_component(_ep, _md_alloc, parent_cap, "custom");

		// Create list element to where the virtual object is stored
		Region_map_info *rm_info = new (_md_alloc) Region_map_info(*new_region_map);

		// Insert list element into list
		Genode::Lock::Guard lock(_infos_lock);
		_region_map_infos.insert(rm_info);

		if(verbose_debug) Genode::log("  result: ", new_region_map->cap());
		return new_region_map->cap();
	}

	/**
	 * Destroying the virtual Region map, its real counter part, and the list element it was managed in
	 *
	 * XXX Untested! During the implementation time, the destroy method was not working.
	 */
	void destroy(Genode::Capability<Genode::Region_map> region_map_cap)
	{
		if(verbose_debug) Genode::log("Rm::\033[33m", __func__, "\033[0m(", region_map_cap, ")");

		// Find list element for the given Capability
		Genode::Lock::Guard lock (_infos_lock);
		Region_map_info *rms_info = _region_map_infos.first();
		if(rms_info) rms_info = rms_info->find_by_cap(region_map_cap);

		// If found, delete everything concerning this list element
		if(rms_info)
		{
			if(verbose_debug) Genode::log("  deleting ", rms_info->region_map.cap());

			Genode::error("Issuing Rm_session::destroy, which is bugged and hangs up.");
			// Destroy real Region_map from parent
			_parent_rm.destroy(rms_info->region_map.parent_cap());

			// Destroy virtual Region_map
			Genode::destroy(_md_alloc, &rms_info->region_map);

			// Remove and destroy list element
			_region_map_infos.remove(rms_info);
			Genode::destroy(_md_alloc, rms_info);
		}
		else
		{
			Genode::error("No Region map with ", region_map_cap, " found!");
		}
	}
};

/**
 * Virtual Root session object to intercept Rm session object creation
 * This enables the Rtcr component to monitor capabilities created for Rm session objects
 */
class Rtcr::Rm_root : public Genode::Root_component<Rm_session_component>
{
private:
	/**
	 * Enable log output for debugging
	 */
	static constexpr bool verbose_debug = rm_root_verbose_debug;

	/**
	 * Environment of Rtcr; is forwarded to a created session object
	 */
	Genode::Env                   &_env;
	/**
	 * Allocator for session objects and monitoring list elements
	 */
	Genode::Allocator             &_md_alloc;
	/**
	 * Entrypoint for managing session objects
	 */
	Genode::Entrypoint            &_ep;
	/**
	 * Lock for infos list
	 */
	Genode::Lock                   _infos_lock;
	/**
	 * List for monitoring Rm session objects
	 */
	Genode::List<Rm_session_info>  _rms_infos;

protected:
	Rm_session_component *_create_session(const char *args)
	{
		if(verbose_debug) Genode::log("Rm_root::\033[33m", __func__, "\033[0m(", args,")");
		// Create virtual Rm_session
		Rm_session_component *new_rms =
				new (md_alloc()) Rm_session_component(_env, _md_alloc, _ep);

		// Create and insert list element
		Rm_session_info *new_rms_info =
				new (md_alloc()) Rm_session_info(*new_rms, args);
		Genode::Lock::Guard lock(_infos_lock);
		_rms_infos.insert(new_rms_info);

		return new_rms;
	}

	void _destroy_session(Rm_session_component *session)
	{
		// Find and destroy list element
		Rm_session_info *rms_info = _rms_infos.first();
		if(rms_info) rms_info = rms_info->find_by_ptr(session);
		if(rms_info)
		{
			_rms_infos.remove(rms_info);
			destroy(_md_alloc, rms_info);
		}

		// Destroy virtual Rm_session
		destroy(_md_alloc, session);
	}

public:
	Rm_root(Genode::Env &env, Genode::Allocator &md_alloc, Genode::Entrypoint &session_ep)
	:
		Root_component<Rm_session_component>(session_ep, md_alloc),
		_env        (env),
		_md_alloc   (md_alloc),
		_ep         (session_ep),
		_infos_lock (),
		_rms_infos  ()
	{
		if(verbose_debug) Genode::log("\033[33m", __func__, "\033[0m");
	}

    ~Rm_root()
    {
        Rm_session_info *info = nullptr;

        while((info = _rms_infos.first()))
        {
            _rms_infos.remove(info);
            Genode::destroy(_md_alloc, info);
        }

        if(verbose_debug) Genode::log("\033[33m", __func__, "\033[0m");
    }

	Genode::List<Rm_session_info> &rms_infos() { return _rms_infos; }
    void rms_infos(Genode::List<Rm_session_info> &infos) { _rms_infos = infos; }
};

#endif /* _RTCR_RM_SESSION_H_ */