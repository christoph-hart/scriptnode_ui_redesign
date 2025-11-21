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



void CablePinBase::mouseDown(const MouseEvent& e)
{
	if (draggingEnabled)
	{
		auto root = findParentComponentOfClass<DspNetworkComponent>();

		root->currentlyDraggedCable = new DraggedCable(this);
		root->addAndMakeVisible(root->currentlyDraggedCable);
	}
}

void CablePinBase::mouseDrag(const MouseEvent& e)
{
	if (draggingEnabled)
	{
		ZoomableViewport::checkDragScroll(e, false);

		auto root = findParentComponentOfClass<DspNetworkComponent>();

		auto rootPos = root->getLocalPoint(this, e.getPosition());
		root->currentlyDraggedCable->setTargetPosition(rootPos);
	}
}

void CablePinBase::mouseUp(const MouseEvent& e)
{
	if (draggingEnabled)
	{
		ZoomableViewport::checkDragScroll(e, true);

		auto root = findParentComponentOfClass<DspNetworkComponent>();

		if(auto dst = root->currentlyDraggedCable->hoveredPin)
		{
			addConnection(dst);
		}

		root->currentlyDraggedCable = nullptr;
	}
}

void CablePinBase::setEnableDragging(bool shouldBeDragable)
{
	draggingEnabled = shouldBeDragable;
	setMouseCursor(draggingEnabled ? MouseCursor::CrosshairCursor : MouseCursor::NormalCursor);
}

void CablePinBase::addConnection(const WeakPtr dst)
{
	auto parameterId = dst->getTargetParameterId();
	auto nodeId = dst->getNodeTree()[PropertyIds::ID].toString();

	dst->data.setProperty(PropertyIds::Automated, true, um);

	ValueTree c(PropertyIds::Connection);

	c.setProperty(PropertyIds::NodeId, nodeId, nullptr);
	c.setProperty(PropertyIds::ParameterId, parameterId, nullptr);

	auto connections = getConnectionTree();
	connections.addChild(c, -1, um);
}

bool ParameterComponent::isOutsideParameter() const
{
	auto p = findParentComponentOfClass<NodeComponent>();
	jassert(p != nullptr);

	return p != nullptr && p->getValueTree().getChildWithName(PropertyIds::Parameters) != data.getParent();
}

}