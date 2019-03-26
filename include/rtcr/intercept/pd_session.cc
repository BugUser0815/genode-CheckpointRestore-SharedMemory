/*
 * \brief  Intercepting Pd session
 * \author Denis Huber
 * \date   2016-08-03
 */

#include "pd_session.h"

using namespace Rtcr;


Pd_session_component::Pd_session_component(Genode::Rpc_entrypoint   &ep,
		                     Genode::Session::Resources         resources,
		                     Genode::Session::Label      const &label,
		                     Genode::Session::Diag              diag,
		                     Genode::Range_allocator  &phys_alloc,
		                     Genode::Region_map       &local_rm,
		                     char const       *args,
		                     Genode::Range_allocator  &core_mem)
:
	Session_object(ep, resources, label, diag),
	_ep(ep),
	_parent_pd(_parent),
	_bootstrap_phase(false),
	_parent_state ("", _bootstrap_phase),
			_constrained_md_ram_alloc(*this, _ram_quota_guard(), _cap_quota_guard()),
			_constrained_core_ram_alloc(_ram_quota_guard(), _cap_quota_guard(), core_mem),
			_md_alloc(_constrained_md_ram_alloc, local_rm),
			_signal_broker(_sliced_heap, signal_ep, signal_ep),
			_ram_ds_factory(ep, phys_alloc, phys_range, local_rm,
			                _constrained_core_ram_alloc),
			_rpc_cap_factory(_sliced_heap),
			_native_pd(*this, args),
			_address_space(ep, _sliced_heap, pager_ep,
			               virt_range.start, virt_range.size, diag),
			_stack_area (ep, _sliced_heap, pager_ep, 0, stack_area_virtual_size(), diag),
			_linker_area(ep, _sliced_heap, pager_ep, 0, LINKER_AREA_SIZE, diag)
	{
		if (platform()->core_needs_platform_pd() || label != "core") {
			_pd.construct(&_sliced_heap, _label.string());
			_address_space.address_space(&*_pd);
		}
	}


Pd_session_component::~Pd_session_component()
{
	//if(verbose_debug) Genode::log("\033[33m", "~Pd", "\033[0m ", _parent_pd);

	/*_ep.dissolve(_linker_area);
	_ep.dissolve(_stack_area);
	_ep.dissolve(_address_space);*/
}


Pd_session_component *Pd_session_component::find_by_badge(Genode::uint16_t badge)
{
	if(badge == cap().local_name())
		return this;
	Pd_session_component *obj = next();
	return obj ? obj->find_by_badge(badge) : 0;
}


void Pd_session_component::assign_parent(Genode::Capability<Genode::Parent> parent)
{
	if(verbose_debug) Genode::log("Pd::\033[33m", __func__, "\033[0m(", parent,")");

	_parent_pd.assign_parent(parent);
}


bool Pd_session_component::assign_pci(Genode::addr_t addr, Genode::uint16_t bdf)
{
	if(verbose_debug) Genode::log("Pd::\033[33m", __func__, "\033[0m(addr=", addr,", bdf=", bdf,")");

	auto result = _parent_pd.assign_pci(addr, bdf);

	if(verbose_debug) Genode::log("  result: ", result);

	return result;
}

void Pd_session_component::map(Genode::addr_t _addr, Genode::addr_t __addr)
{
	_parent_pd.map(_addr, __addr);
}

Genode::Capability<Genode::Signal_source> Pd_session_component::alloc_signal_source()
{
	if(verbose_debug) Genode::log("Pd::\033[33m", __func__, "\033[0m()");

	auto result_cap = _parent_pd.alloc_signal_source();

	// Create and insert list element to monitor this signal source
	Signal_source_info *new_ss_info = new (_md_alloc) Signal_source_info(result_cap, _bootstrap_phase);
	Genode::Lock::Guard guard(_parent_state.signal_sources_lock);
	_parent_state.signal_sources.insert(new_ss_info);

	if(verbose_debug) Genode::log("  result: ", result_cap);

	return result_cap;
}


void Pd_session_component::free_signal_source(Genode::Capability<Genode::Signal_source> cap)
{
	if(verbose_debug) Genode::log("Pd::\033[33m", __func__, "\033[0m(", cap, ")");

	// Find list element
	Genode::Lock::Guard guard(_parent_state.signal_sources_lock);
	Signal_source_info *ss_info = _parent_state.signal_sources.first();
	if(ss_info) ss_info = ss_info->find_by_badge(cap.local_name());

	// List element found?
	if(ss_info)
	{
		// Remove and destroy list element
		_parent_state.signal_sources.remove(ss_info);
		Genode::destroy(_md_alloc, ss_info);

		// Free signal source
		_parent_pd.free_signal_source(cap);
	}
	else
	{
		Genode::error("No list element found!");
	}
}


Genode::Signal_context_capability Pd_session_component::alloc_context(Signal_source_capability source,
		unsigned long imprint)
{
	if(verbose_debug) Genode::log("Pd::\033[33m", __func__, "\033[0m(source ", source, ", imprint=", Genode::Hex(imprint), ")");

	auto result_cap = _parent_pd.alloc_context(source, imprint);

	// Create and insert list element to monitor this signal context
	Signal_context_info *new_sc_info = new (_md_alloc) Signal_context_info(result_cap, source, imprint, _bootstrap_phase);
	Genode::Lock::Guard guard(_parent_state.signal_contexts_lock);
	_parent_state.signal_contexts.insert(new_sc_info);

	if(verbose_debug) Genode::log("  result: ", result_cap);

	return result_cap;
}


void Pd_session_component::free_context(Genode::Signal_context_capability cap)
{
	if(verbose_debug) Genode::log("Pd::\033[33m", __func__, "\033[0m(", cap, ")");

	// Find list element
	Genode::Lock::Guard guard(_parent_state.signal_contexts_lock);
	Signal_context_info *sc_info = _parent_state.signal_contexts.first();
	if(sc_info) sc_info = sc_info->find_by_badge(cap.local_name());

	// List element found?
	if(sc_info)
	{
		// Remove and destroy list element
		_parent_state.signal_contexts.remove(sc_info);
		Genode::destroy(_md_alloc, sc_info);

		// Free signal context
		_parent_pd.free_context(cap);
	}
	else
	{
		Genode::error("No list element found!");
	}
}


void Pd_session_component::submit(Genode::Signal_context_capability context, unsigned cnt)
{
	if(verbose_debug) Genode::log("Pd::\033[33m", __func__, "\033[0m(context ", context, ", cnt=", cnt,")");

	_parent_pd.submit(context, cnt);
}


Genode::Native_capability Pd_session_component::alloc_rpc_cap(Genode::Native_capability ep)
{
	if(verbose_debug) Genode::log("Pd::\033[33m", "alloc_rpc_cap", "\033[0m(", ep, ")");

	auto result_cap = _parent_pd.alloc_rpc_cap(ep);

	// Create and insert list element to monitor this native_capability
	Native_capability_info *new_nc_info = new (_md_alloc) Native_capability_info(result_cap, ep, _bootstrap_phase);
	Genode::Lock::Guard guard(_parent_state.native_caps_lock);
	_parent_state.native_caps.insert(new_nc_info);

	if(verbose_debug) Genode::log("  result: ", result_cap);

	return result_cap;
}


void Pd_session_component::free_rpc_cap(Genode::Native_capability cap)
{
	if(verbose_debug) Genode::log("Pd::\033[33m", __func__, "\033[0m(", cap,")");

	// Find list element
	Genode::Lock::Guard guard(_parent_state.native_caps_lock);
	Native_capability_info *nc_info = _parent_state.native_caps.first();
	if(nc_info) nc_info = nc_info->find_by_native_badge(cap.local_name());

	// List element found?
	if(nc_info)
	{
		// Remove and destroy list element
		_parent_state.native_caps.remove(nc_info);
		Genode::destroy(_md_alloc, nc_info);

		// Free native capability
		_parent_pd.free_rpc_cap(cap);
	}
	else
	{
		Genode::error("No list element found!");
	}
}


Genode::Capability<Genode::Region_map> Pd_session_component::address_space()
{
	if(verbose_debug) Genode::log("Pd::\033[33m", __func__, "\033[0m()");

	auto result = _address_space.Rpc_object<Genode::Region_map>::cap();

	if(verbose_debug) Genode::log("  result: ", result);

	return result;
}


Genode::Capability<Genode::Region_map> Pd_session_component::stack_area()
{
	if(verbose_debug) Genode::log("Pd::\033[33m", __func__, "\033[0m()");

	auto result = _stack_area.Rpc_object<Genode::Region_map>::cap();

	if(verbose_debug) Genode::log("  result: ", result);

	return result;
}


Genode::Capability<Genode::Region_map> Pd_session_component::linker_area()
{
	if(verbose_debug) Genode::log("Pd::\033[33m", __func__, "\033[0m()");

	auto result = _linker_area.Rpc_object<Genode::Region_map>::cap();

	if(verbose_debug) Genode::log("  result: ", result);

	return result;
}

void Pd_session_component::ref_account(Genode::Capability<Genode::Pd_session> cap)
{
	_parent_pd.ref_account(cap);
}

void Pd_session_component::transfer_quota(Genode::Capability<Genode::Pd_session> cap, Genode::Cap_quota quota)
{
	Genode::log("intercepted cap quota ",quota.value);
	Genode::log("intercepted cap quota ",_parent_pd.cap_quota().value);
	Genode::log(cap);
	_parent_pd.transfer_quota(cap, quota);
}

void Pd_session_component::transfer_quota(Genode::Capability<Genode::Pd_session> cap, Genode::Ram_quota quota)
{
	Genode::log("intercepted ram quota ",quota.value);
	Genode::log("intercepted ram quota ",_parent_pd.ram_quota().value);
	_parent_pd.transfer_quota(cap, quota);
}

Genode::Cap_quota Pd_session_component::cap_quota() const
{
	auto result = _parent_pd.cap_quota();

	return result;
}

Genode::Cap_quota Pd_session_component::used_caps() const
{
	auto result = _parent_pd.used_caps();

	return result;
}

Genode::Ram_quota Pd_session_component::ram_quota() const
{
	auto result = _parent_pd.ram_quota();

	return result;
}

Genode::Ram_quota Pd_session_component::used_ram() const
{
	auto result = _parent_pd.used_ram();

	return result;
}

Genode::Ram_dataspace_capability Pd_session_component::alloc(Genode::size_t size, Genode::Cache_attribute attr)
{
	auto result = _parent_pd.alloc(size, attr);

	return result;
}

void Pd_session_component::free(Genode::Ram_dataspace_capability ram_cap)
{
	_parent_pd.free(ram_cap);
}

Genode::size_t Pd_session_component::dataspace_size(Genode::Ram_dataspace_capability cap) const
{
	auto result = _parent_pd.dataspace_size(cap);

	return result;
}

Genode::Capability<Genode::Pd_session::Native_pd> Pd_session_component::native_pd()
{
	if(verbose_debug) Genode::log("Pd::\033[33m", __func__, "\033[0m()");

	auto result = _parent_pd.native_pd();

	if(verbose_debug) Genode::log("  result: ", result);

	return result;
}

Pd_session_component *Pd_root::_create_session(const char *args)
{
	Genode::log("BUMMMM");
	if(verbose_debug) Genode::log("Pd_root::\033[33m", __func__, "\033[0m(", args,")");

	/// Extracting label from args
	char label_buf[128];
	Genode::Arg label_arg = Genode::Arg_string::find_arg(args, "label");
	label_arg.string(label_buf, sizeof(label_buf), "");

	/* Revert ram_quota calculation, because the monitor needs the original session creation argument
	char ram_quota_buf[32];
	char readjusted_args[160];
	Genode::strncpy(readjusted_args, args, sizeof(readjusted_args));

	Genode::size_t readjusted_ram_quota = Genode::Arg_string::find_arg(readjusted_args, "ram_quota").ulong_value(0);
	readjusted_ram_quota = readjusted_ram_quota + sizeof(Pd_session_component) + md_alloc()->overhead(sizeof(Pd_session_component));

	Genode::snprintf(ram_quota_buf, sizeof(ram_quota_buf), "%zu", readjusted_ram_quota);
	Genode::Arg_string::set_arg(readjusted_args, sizeof(readjusted_args), "ram_quota", ram_quota_buf);*/

	Genode::Session::Diag diag{};
	// Create custom Pd_session
	Pd_session_component *new_session =
			new (_md_alloc) Pd_session_component(_env, _md_alloc, _ep, args, args, _bootstrap_phase, Genode::session_resources_from_args("cap_quota=50,ram_quota=100000"), diag);

	Genode::Lock::Guard lock(_objs_lock);
	_session_rpc_objs.insert(new_session);

	return new_session;
}


void Pd_root::_upgrade_session(Pd_session_component *session, const char *upgrade_args)
{
	if(verbose_debug) Genode::log("Pd_root::\033[33m", __func__, "\033[0m(session ", session->cap(),", args=", upgrade_args,")");

	char ram_quota_buf[32];
	char new_upgrade_args[160];

	Genode::strncpy(new_upgrade_args, session->parent_state().upgrade_args.string(), sizeof(new_upgrade_args));

	Genode::size_t ram_quota = Genode::Arg_string::find_arg(new_upgrade_args, "ram_quota").ulong_value(0);
	Genode::size_t extra_ram_quota = Genode::Arg_string::find_arg(upgrade_args, "ram_quota").ulong_value(0);
	ram_quota += extra_ram_quota;

	Genode::snprintf(ram_quota_buf, sizeof(ram_quota_buf), "%zu", ram_quota);
	Genode::Arg_string::set_arg(new_upgrade_args, sizeof(new_upgrade_args), "ram_quota", ram_quota_buf);

	session->parent_state().upgrade_args = new_upgrade_args;

	_env.parent().upgrade(Genode::Parent::Env::pd(), upgrade_args);
}


void Pd_root::_destroy_session(Pd_session_component *session)
{
	if(verbose_debug) Genode::log("Pd_root::\033[33m", __func__, "\033[0m(session ", session->cap(),")");

	_session_rpc_objs.remove(session);
	Genode::destroy(_md_alloc, session);
}


Pd_root::Pd_root(Genode::Env &env, Genode::Allocator &md_alloc, Genode::Entrypoint &session_ep,
		bool &bootstrap_phase)
:
	Root_component<Pd_session_component>(session_ep, md_alloc),
	_env              (env),
	_md_alloc         (md_alloc),
	_ep               (session_ep),
	_bootstrap_phase  (bootstrap_phase),
	_objs_lock        (),
	_session_rpc_objs ()
{
	if(verbose_debug) Genode::log("\033[33m", __func__, "\033[0m");
}


Pd_root::~Pd_root()
{
	while(Pd_session_component *obj = _session_rpc_objs.first())
	{
		_session_rpc_objs.remove(obj);
		Genode::destroy(_md_alloc, obj);
	}

	if(verbose_debug) Genode::log("\033[33m", __func__, "\033[0m");
}
