/*
 * \brief  Structure for storing Region_map attachments
 * \author Denis Huber
 * \date   2016-10-25
 */

#ifndef _RTCR_STORED_REGION_INFO_H_
#define _RTCR_STORED_REGION_INFO_H_

/* Genode includes */
#include <util/list.h>

/* Rtcr includes */
#include "../online_storage/attached_region_info.h"
#include "../offline_storage/stored_info_structs.h"


namespace Rtcr {
	struct Stored_attached_region_info;
}

struct Rtcr::Stored_attached_region_info : Stored_normal_info, Genode::List<Stored_attached_region_info>::Element
{
	Genode::uint16_t                 const attached_ds_badge;
	Genode::Ram_dataspace_capability const memory_content;
	Genode::size_t const size;
	Genode::off_t  const offset;
	Genode::addr_t const rel_addr;
	bool           const executable;

	Stored_attached_region_info(Attached_region_info &info, Genode::addr_t kcap, Genode::Ram_dataspace_capability copy_ds_cap)
	:
		Stored_normal_info(kcap,
				info.attached_ds_cap.local_name(),
				info.bootstrapped),
		attached_ds_badge (info.attached_ds_cap.local_name()),
		memory_content    (copy_ds_cap),
		size       (info.size),
		offset     (info.offset),
		rel_addr   (info.rel_addr),
		executable (info.executable)
	{ }

	Stored_attached_region_info *find_by_addr(Genode::addr_t addr)
	{
		if((addr >= rel_addr) && (addr <= rel_addr + size))
			return this;
		Stored_attached_region_info *info = next();
		return info ? info->find_by_addr(addr) : 0;
	}

	void print(Genode::Output &output) const
	{
		using Genode::Hex;

		Stored_normal_info::print(output);
		Genode::print(output, ", attached_ds_badge=", attached_ds_badge, " ");
		Genode::print(output, " [", Hex(rel_addr, Hex::PREFIX, Hex::PAD));
		Genode::print(output, ", ", Hex(rel_addr + size - offset, Hex::PREFIX, Hex::PAD));
		Genode::print(output, ") exec=", executable);
	}

};

#endif /* _RTCR_STORED_REGION_INFO_COMPONENT_H_ */
