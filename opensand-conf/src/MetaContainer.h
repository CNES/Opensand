/*
 *
 * OpenSAND is an emulation testbed aiming to represent in a cost effective way a
 * satellite telecommunication system for research and engineering activities.
 *
 *
 * Copyright © 2020 Viveris Technologies
 * Copyright © 2020 TAS
 * Copyright © 2020 CNES
 *
 *
 * This file is part of the OpenSAND testbed.
 *
 *
 * OpenSAND is free software : you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY, without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see http://www.gnu.org/licenses/.
 *
 */

/**
 * @file MetaContainer.h
 * @brief Base class of all metamodel containers.
 */

#ifndef OPENSAND_CONF_META_CONTAINER_H
#define OPENSAND_CONF_META_CONTAINER_H

#include <memory>
#include <string>
#include <vector>

#include <MetaElement.h>

using std::shared_ptr;
using std::weak_ptr;
using std::string;
using std::vector;

namespace OpenSANDConf
{
	class DataContainer;

	class MetaContainer: public MetaElement
	{
	public:
		friend class MetaModel;
		friend class MetaElement;

		/**
		 * @brief Destructor.
		 */
		virtual ~MetaContainer();

		/**
		 * @brief Compare to another element
		 *
		 * @param  other  Element to compare to
		 *
		 * @return  True if elements are equals, false otherwise
		 */
		virtual bool equal(const MetaElement &other) const override;

	protected:
		/**
		 * @brief Constructor.
		 *
		 * @param  id           The identifier
		 * @param  parent       The parent path
		 * @param  name         The name
		 * @param  description  The description
		 * @param  types        The types list
		 */
		MetaContainer(const string &id, const string &parent, const string &name, const string &description, weak_ptr<const MetaTypesList> types);

		/**
		 * @brief Contructor by copy.
		 *
		 * @param  other  The object to copy
		 * @param  types  The types list
		 */
		MetaContainer(const MetaContainer &other, weak_ptr<const MetaTypesList> types);

		/**
		 * @brief Clone the current object.
		 *
		 * @param  types  The types list
		 *
		 * @return The cloned object
		 */
		virtual shared_ptr<MetaElement> clone(weak_ptr<const MetaTypesList> types) const = 0;

		/**
		 * @brief Create a datamodel element.
		 *
		 * @param  types  The types list
		 *
		 * @return  The new datamodel element if succeeds, nullptr otherwise
		 */
		virtual shared_ptr<DataElement> createData(shared_ptr<DataTypesList> types) const = 0;

		/**
		 * @brief Create a datamodel element for each items and add it to datamodel container.
		 *
		 * @param  types  The types list
		 * @param  container  The datamodel container to extend
		 */
		virtual void createAndAddDataItems(shared_ptr<DataTypesList> types, shared_ptr<DataContainer> container) const;

		/**
		 * @brief Get the meta types list.
		 *
		 * @return The meta types list
		 */
		weak_ptr<const MetaTypesList> getTypes() const;

		/**
		 * @brief Get the items.
		 *
		 * @return  The items
		 */
		virtual const vector<shared_ptr<MetaElement>> &getItems() const;

		/**
		 * @brief Get an identified item.
		 *
		 * @param  id  The identifier
		 *
		 * @return  The item if found, nullptr otherwise
		 */
		virtual shared_ptr<MetaElement> getItem(const string &id) const;

		/**
		 * @brief Add an item.
		 *
		 * @param  item  The item to add
		 */
		void addItem(shared_ptr<MetaElement> item);

	private:
		weak_ptr<const MetaTypesList> types;
		vector<shared_ptr<MetaElement>> items;
	};
}

#endif // OPENSAND_CONF_META_CONTAINER_H
