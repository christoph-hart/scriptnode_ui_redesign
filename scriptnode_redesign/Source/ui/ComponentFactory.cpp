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



void DummyComplexDataProvider::Editor::checkBounds(Rectangle<int>& bounds, const Rectangle<int>& previousBounds, const Rectangle<int>& limits, bool isStretchingTop, bool isStretchingLeft, bool isStretchingBottom, bool isStretchingRight)
{
	auto vTree = pSource->getValueTree();

	auto w = jlimit(100, 1000, bounds.getWidth());
	auto h = jlimit(50, 1000, bounds.getHeight());

	bounds.setWidth(w);
	bounds.setHeight(h);

	findParentComponentOfClass<NodeComponent>()->setFixSize(bounds.expanded(10));
}

void NodeResizer::paint(Graphics& g)
{
	Path p;
	p.startNewSubPath(1.0, 0.0);
	p.lineTo(1.0, 1.0);
	p.lineTo(0.0, 1.0);
	p.closeSubPath();

	auto over = isMouseOver(true);

	PathFactory::scalePath(p, getLocalBounds().toFloat().reduced(over ? 2.f : 3.0f));

	auto c = Helpers::getNodeColour(findParentComponentOfClass<ParameterSourceObject>()->getValueTree());
	g.setColour(c);
	g.fillPath(p);
}

void NodeResizer::mouseDrag(const MouseEvent& e)
{
	ResizableCornerComponent::mouseDrag(e);
	ContainerComponent::expandParentsRecursive(*this, {}, false);
}

void NodeResizer::mouseUp(const MouseEvent& e)
{
	auto parent = findParentComponentOfClass<NodeComponent>();

	Helpers::updateBounds(parent->getValueTree(), parent->getBoundsInParent(), parent->getUndoManager());

	Helpers::fixOverlap(Helpers::getCurrentRoot(parent->getValueTree()), parent->getUndoManager(), false);
}

}