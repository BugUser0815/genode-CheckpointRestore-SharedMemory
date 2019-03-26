/*
 * \brief  Intercepting Pd session
 * \author Denis Huber
 * \date   2016-08-03
 */

#ifndef _RTCR_PD_SESSION_H_
#define _RTCR_PD_SESSION_H_

/* Genode includes */
#include <util/reconstructible.h>
#include <base/allocator_guard.h>
#include <base/session_object.h>
#include <base/registry.h>
#include <base/heap.h>
#include <pd_session/pd_session.h>
#include <util/arg_string.h>

/* Rtcr includes */
#include "../online_storage/pd_session_info.h"
#include "region_map_component.h"

namespace Rtcr {
	class Pd_session_component;
	class Pd_root;

	constexpr bool pd_verbose_debug = true;
	constexpr bool pd_root_verbose_debug = true;
}

/**
 * Custom RPC session object to intercept its creation, modification, and destruction through its interface
 */
class Rtcr::Pd_session_component : public Genode::Session_object<Genode::Pd_session>,
                                   private Genode::List<Pd_session_component>::Element
{
private:
	friend class Genode::List<Rtcr::Pd_session_component>;
	/**
	 * Enable log output for debugging
	 */
	static constexpr bool verbose_debug = pd_verbose_debug;

	Genode::Pd_connection _parent_pd;

	Pd_session_info _parent_state;

	bool &_bootstrap_phase;

	Genode::Rpc_entrypoint            &_ep;
	Genode::Constrained_ram_allocator  _constrained_md_ram_alloc;
	Genode::Sliced_heap                _md_alloc;
	Genode::Capability<Genode::Parent>         _parent { };

	Region_map_component _address_space;
	Region_map_component _stack_area;
	Region_map_component _linker_area;


public:
	Pd_session_component(Genode::Rpc_entrypoint   &ep,
		                     Genode::Rpc_entrypoint   &signal_ep,
		                     Genode::Session::Resources         resources,
		                     Genode::Session::Label      const &label,
		                     Genode::Session::Diag              diag,
		                     Genode::Range_allocator  &phys_alloc,
		                     Genode::Region_map       &local_rm,
		                     char const       *args,
		                     Genode::Range_allocator  &core_mem);
	~Pd_session_component();

	Genode::Pd_session_capability parent_cap() { return _parent_pd.cap(); }

	Region_map_component &address_space_component() { return _address_space; }
	Region_map_component const &address_space_component() const { return _address_space; }

	Region_map_component &stack_area_component() { return _stack_area; }
	Region_map_component const &stack_area_component() const { return _stack_area; }

	Region_map_component &linker_area_component() { return _linker_area; }
	Region_map_component const &linker_area_component() const { return _linker_area; }

	Pd_session_info &parent_state() { return _parent_state; }
	Pd_session_info const &parent_state() const { return _parent_state;}

	Pd_session_component *find_by_badge(Genode::uint16_t badge);

	using Genode::List<Rtcr::Pd_session_component>::Element::next;

	/**************************
	 ** Pd_session interface **
	 **************************/

	void assign_parent(Genode::Capability<Genode::Parent> parent) override;

	bool assign_pci(Genode::addr_t addr, Genode::uint16_t bdf) override;

	void map(Genode::addr_t _addr, Genode::addr_t __addr) override;

	Signal_source_capability alloc_signal_source() override;
	void free_signal_source(Signal_source_capability cap) override;

	Genode::Signal_context_capability alloc_context(Signal_source_capability source,
			unsigned long imprint) override;
	void free_context(Genode::Signal_context_capability cap) override;

	void submit(Genode::Signal_context_capability context, unsigned cnt) override;

	Genode::Native_capability alloc_rpc_cap(Genode::Native_capability ep) override;
	void free_rpc_cap(Genode::Native_capability cap) override;
	/**
	 * Return custom address space
	 */
	Genode::Capability<Genode::Region_map> address_space() override;
	/**
	 * Return custom stack area
	 */
	Genode::Capability<Genode::Region_map> stack_area() override;
	/**
	 * Return custom linker area
	 */
	Genode::Capability<Genode::Region_map> linker_area() override;

	void ref_account(Genode::Capability<Genode::Pd_session>) override;

	void transfer_quota(Genode::Capability<Genode::Pd_session>, Genode::Cap_quota) override;
	void transfer_quota(Genode::Capability<Genode::Pd_session>, Genode::Ram_quota) override;

	Genode::Cap_quota cap_quota() const override;

	Genode::Cap_quota used_caps() const override;

	Genode::Ram_quota ram_quota() const override;

	Genode::Ram_quota used_ram() const override;

	Genode::Ram_dataspace_capability alloc(Genode::size_t, Genode::Cache_attribute) override;

	void free(Genode::Ram_dataspace_capability) override;

	Genode::size_t dataspace_size(Genode::Ram_dataspace_capability) const override;
	Genode::Capability<Native_pd> native_pd() override;

};


/**
 * Custom root RPC object to intercept session RPC object creation, modification, and destruction through the root interface
 */
class Rtcr::Pd_root : public Genode::Root_component<Pd_session_component>
{
private:
	/**
	 * Enable log output for debugging
	 */
	static constexpr bool verbose_debug = pd_root_verbose_debug;

	/**
	 * Environment of Rtcr; is forwarded to a created session object
	 */
	Genode::Env        &_env;
	/**
	 * Allocator for session objects and monitoring list elements
	 */
	Genode::Allocator  &_md_alloc;
	/**
	 * Entrypoint for managing session objects
	 */
	Genode::Entrypoint &_ep;
	/**
	 * Reference to Target_child's bootstrap phase
	 */
	bool               &_bootstrap_phase;
	/**
	 * Lock for infos list
	 */
	Genode::Lock        _objs_lock;
	/**
	 * List for monitoring session objects
	 */
	Genode::List<Pd_session_component> _session_rpc_objs;

protected:
	
	void _upgrade_session(Pd_session_component *session, const char *upgrade_args);
	void _destroy_session(Pd_session_component *session);

public:
	Pd_session_component *_create_session(const char *args);
	Pd_root(Genode::Env &env, Genode::Allocator &md_alloc, Genode::Entrypoint &session_ep,
			bool &bootstrap_phase);
    ~Pd_root();

	Genode::List<Pd_session_component> &session_infos() { return _session_rpc_objs; }
};

#endif /* _RTCR_PD_SESSION_H_ */
