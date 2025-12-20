/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licenses for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licensing:
*
*   http://www.hise.audio/
*
*   HISE is based on the JUCE library,
*   which must be separately licensed for closed source applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

namespace scriptnode {
using namespace hise;
using namespace juce;



LockedContainerComponent::LockedContainerComponent(Lasso* l, const ValueTree& v, UndoManager* um_) :
	ProcessNodeComponent(l, v, um_),
	gotoButton("goto", nullptr, *this)
{
	setOpaque(true);
	for (auto cn : v.getChildWithName(PropertyIds::Nodes))
	{
		if (Helpers::getFactoryPath(cn).second == "locked_mod")
		{
			auto mt = cn.getOrCreateChildWithName(PropertyIds::ModulationTargets, um);
			addAndMakeVisible(modOutputs.add(new ModOutputComponent(mt, um)));

			// TODO: add support for more?
			break;
		}
	}

	setFixSize({});

	gotoButton.onClick = [this]()
	{
		NetworkParent::setNewContent(this, getValueTree());
	};

	header.addAndMakeVisible(gotoButton);
}

Component* LockedContainerComponent::createPopupComponent()
{
	auto p = NetworkParent::getNetworkParent(this);
	return new NetworkParent::Map(getValueTree(), *p, {});
}

}