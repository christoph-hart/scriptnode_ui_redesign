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

#include <JuceHeader.h>

namespace scriptnode {
using namespace hise;
using namespace juce;

Component* DummyBody::createDummyComponent(const String& path)
{
	auto nodeType = path.fromFirstOccurrenceOf(".", false, false);

#define RETURN_IMAGE(name) if (nodeType == #name) return new DummyBody(ImageCache::getFromMemory(BinaryData::name##_png, BinaryData::name##_png##Size));

	RETURN_IMAGE(expr);
	RETURN_IMAGE(cable_expr);
	RETURN_IMAGE(pma_unscaled);
	RETURN_IMAGE(snex_shaper);
	RETURN_IMAGE(one_pole);
	RETURN_IMAGE(oscillator);
	RETURN_IMAGE(xfader);
	RETURN_IMAGE(filter);
	RETURN_IMAGE(pma);
	RETURN_IMAGE(table);
	RETURN_IMAGE(simple_ar);
	RETURN_IMAGE(ramp);
	RETURN_IMAGE(peak);

#undef RETURN_IMAGE

	return nullptr;
}

DummyBody::DummyBody(const Image& img_) :
	img(img_)
{
	setSize(img.getWidth() / 2, img.getHeight() / 2);
}

void DummyBody::paint(Graphics& g)
{
	g.drawImageWithin(img, 0, 0, getWidth(), getHeight(), RectanglePlacement::centred);
}

std::pair<juce::String, juce::String> Helpers::getFactoryPath(const ValueTree& v)
{
	auto p = v[PropertyIds::FactoryPath].toString();
	auto f = p.upToFirstOccurrenceOf(".", false, false);
	auto n = p.fromFirstOccurrenceOf(".", false, false);

	return { f, n };
}

juce::var Helpers::getNodeProperty(const ValueTree& v, const Identifier& id, const var& defaultValue)
{
	jassert(v.getType() == PropertyIds::Node);

	auto prop = v.getChildWithName(PropertyIds::Properties);

	for (auto p : prop)
	{
		if (p[PropertyIds::ID].toString() == id.toString())
			return p[PropertyIds::Value];
	}

	return defaultValue;
}

juce::String Helpers::getUniqueId(const String& prefix, const ValueTree& rootTree)
{
	int suffix = 0;

	valuetree::Helpers::forEach(rootTree, [&](const ValueTree& v)
	{
		if(v.getType() == PropertyIds::Node)
		{
			auto thisId = v[PropertyIds::ID].toString();

			if(thisId.contains(prefix))
				suffix++;
		}

		return false;
	});

	if(suffix == 0)
		return prefix;
	else
		return prefix + String(suffix+1);
}

void Helpers::setNodeProperty(ValueTree& v, const Identifier& propertyName, const var& value, UndoManager* um)
{
	jassert(v.getType() == PropertyIds::Node);

	auto prop = v.getOrCreateChildWithName(PropertyIds::Properties, um);

	for (auto p : prop)
	{
		if (p[PropertyIds::ID].toString() == propertyName.toString())
		{
			p.setProperty(PropertyIds::Value, value, um);
			return;
		}
	}

	ValueTree np(PropertyIds::Property);
	np.setProperty(PropertyIds::ID, propertyName.toString(), nullptr);
	np.setProperty(PropertyIds::Value, value, nullptr);

	prop.addChild(np, -1, um);
}

bool Helpers::hasRoutableSignal(const ValueTree& v)
{
	static const StringArray routableNodes = {
		"send",
		"receive"
	};

	return routableNodes.contains(getFactoryPath(v).second);
}

bool Helpers::isProcessNode(const ValueTree& v)
{
	return DataBaseHelpers::isSignalNode(v);
}

bool Helpers::isVerticalContainer(const ValueTree& v)
{
	return !Helpers::getNodeProperty(v, PropertyIds::IsVertical, true);
}

bool Helpers::isContainerNode(const ValueTree& v)
{
	jassert(v.getType() == PropertyIds::Node);
	return getFactoryPath(v).first == "container";
}

bool Helpers::isRootNode(const ValueTree& v)
{
	return v.getParent().getType() == PropertyIds::Network;
}

bool Helpers::shouldBeVertical(const ValueTree& container)
{
	jassert(container.getType() == PropertyIds::Node);

	if (isVerticalContainer(container))
		return true;

	auto p = getFactoryPath(container);

	static const StringArray verticalContainers = {
		"split",
		"multi"
	};

	if (verticalContainers.contains(p.second))
		return true;

	return false;
}

bool Helpers::isFoldedRecursive(const ValueTree& v)
{
	if(v.getType() == PropertyIds::Network || !v.isValid())
		return false;

	if(v.getType() == PropertyIds::Node)
	{
		if(v[PropertyIds::Folded])
			return true;
	}

	return isFoldedRecursive(valuetree::Helpers::findParentWithType(v, PropertyIds::Node));
}

bool Helpers::isProcessingSignal(const ValueTree& v)
{
	auto bypassed = (bool)v[PropertyIds::Bypassed];

	if(isContainerNode(v))
	{
		
		auto container = getFactoryPath(v).second;

		if(container == "offline")
			return false;

		static const StringArray switchers = {
			"midichain",
			"oversample",
			"fix32_block"
		};

		if(switchers.contains(container))
			return true;

		return !bypassed;
	}

	return isProcessNode(v) && !bypassed;
	

}

void Helpers::translatePosition(ValueTree& node, Point<int> delta, UndoManager* um)
{
	auto pos = getPosition(node);
	auto newPos = pos.translated(delta.getX(), delta.getY());

	node.setProperty(UIPropertyIds::x, newPos.getX(), um);
	node.setProperty(UIPropertyIds::y, newPos.getY(), um);
}

void Helpers::setMinPosition(Rectangle<int>& b, Point<int> minOffset)
{
	b.setPosition(jmax<int>(minOffset.getX(), b.getX()), jmax<int>(minOffset.getY(), b.getY()));
}

bool Helpers::hasDefinedBounds(const ValueTree& v)
{
	return v.hasProperty(UIPropertyIds::width) || v.hasProperty(UIPropertyIds::height);
}

void Helpers::updateBounds(ValueTree& v, Rectangle<int> newBounds, UndoManager* um)
{
	jassert(v.getType() == PropertyIds::Node);

	auto prevBounds = getBounds(v, false);

	if (prevBounds != newBounds)
	{
		String msg;
		msg << getHeaderTitle(v);
		msg << prevBounds.toString() << " -> " << newBounds.toString();
		DBG(msg);

		v.setProperty(UIPropertyIds::x, newBounds.getX(), um);
		v.setProperty(UIPropertyIds::y, newBounds.getY(), um);

		auto updateSize = !v[PropertyIds::Folded];

		if (updateSize)
		{
			v.setProperty(UIPropertyIds::width, newBounds.getWidth(), um);
			v.setProperty(UIPropertyIds::height, newBounds.getHeight(), um);
		}
	}
}

bool Helpers::isImmediateChildNode(const ValueTree& childNode, const ValueTree& parent)
{
	jassert(childNode.getType() == PropertyIds::Node);
	jassert(parent.getType() == PropertyIds::Node);

	return childNode.getParent().getParent() == parent;
}

void Helpers::fixOverlap(ValueTree v, UndoManager* um, bool sortProcessNodesFirst)
{
	fixOverlapRecursive(v, um, sortProcessNodesFirst);
}

Array<juce::ValueTree> Helpers::getAutomatedChildParameters(ValueTree container)
{
	jassert(isContainerNode(container));

	Array<ValueTree> parameters;
	Array<std::pair<String, String>> matches;

	auto thisParameters = container.getChildWithName(PropertyIds::Parameters);

	for(auto cn: container.getChildWithName(PropertyIds::Nodes))
	{
		auto pTree = cn.getChildWithName(PropertyIds::Parameters);

		for(auto p: pTree)
		{
			if(p[PropertyIds::Automated])
			{
				matches.add({ cn[PropertyIds::ID].toString(), p[PropertyIds::ID].toString() });
			}
		}
	}

	for(auto m: matches)
	{
		valuetree::Helpers::forEachParent(container, [&](const ValueTree& v)
		{
			if(v.getType() == PropertyIds::Node)
			{
				for(auto p: v.getChildWithName(PropertyIds::Parameters))
				{
					for(auto c: p.getChildWithName(PropertyIds::Connections))
					{
						std::pair<String, String> m(c[PropertyIds::NodeId].toString(), c[PropertyIds::ParameterId].toString());

						if(matches.contains(m))
						{
							parameters.addIfNotAlreadyThere(p);
							break;
						}
					}
				}
			}

			return false;
		});
	}

	for(int i = 0; i < parameters.size(); i++)
	{
		if(parameters[i].getParent() == thisParameters)
			parameters.remove(i--);
	}
	
	return parameters;
}

juce::Colour Helpers::getNodeColour(const ValueTree& v)
{
	jassert(v.getType() == PropertyIds::Node);

	if(isRootNode(v))
		return Colour(0xFF305555);

	auto t = getFactoryPath(v);

	if (t.second == "midichain")
		return Colour(0xFFC65638);
	if (t.second == "modchain")
		return Colour(0xffbe952c);

	auto c = (int64)v[PropertyIds::NodeColour];

	if (c != 0)
		return Colour((uint32)c);

	if (isContainerNode(v))
		return Colour(0xFF777777);

	return Colour(0xFF666666);
}

juce::String Helpers::getHeaderTitle(const ValueTree& v)
{
	jassert(v.getType() == PropertyIds::Node);

	auto text = v[PropertyIds::Name].toString();

	if (text.isEmpty())
		text = v[PropertyIds::ID].toString();

	//text << " (" << v[PropertyIds::FactoryPath].toString() << ")";

	return text;
}

juce::String Helpers::getSignalDescription(const ValueTree& container)
{
	jassert(isContainerNode(container));

	auto path = getFactoryPath(container).second;

	auto forceSerial = (bool)container[PropertyIds::Bypassed];

	auto ch =  " (" + String(getNumChannels(container)) + " ch.)";

	if(path == "offline")
		return "No signal in this container";

	if(!forceSerial && path == "midichain")
		return "Signal with MIDI events";

	if(!forceSerial && path == "modchain")
		return "Monophonic Control signal";

	if(path == "split")
		return "Copy & Mix signal" + ch;

	if(path == "multi")
		return "Split multichchannel signal" + ch;

	if(!forceSerial && path.contains("fix"))
		return "Split Audio buffer into 32 samples";

	if(!forceSerial && path.contains("frame"))
		return "Process the signal for each sample";

	return "Signal input" + ch;
}

Point<int> Helpers::getPosition(const ValueTree& v)
{
	auto x = (int)v[UIPropertyIds::x];
	auto y = (int)v[UIPropertyIds::y];

	if (v.getParent().getType() == PropertyIds::Network)
		return { 0, 0 };

	return { x, y };
}

Rectangle<int> Helpers::getBounds(const ValueTree& v, bool includingComment)
{
	auto p = getPosition(v);
	auto w = (int)v[UIPropertyIds::width];
	auto h = (int)v[UIPropertyIds::height];

	if (w == 0)
		w = 200;
	if (h == 0)
		h = 100;

	if (v[PropertyIds::Folded])
	{
		w = GLOBAL_BOLD_FONT().getStringWidth(getHeaderTitle(v)) + 2 * HeaderHeight;

		w = jmax(150, w);

		h = HeaderHeight;

		if (isProcessNode(v) || isContainerNode(v))
			h += SignalHeight;
	}

	includingComment &= v[PropertyIds::Comment].toString().isNotEmpty();

	if(includingComment)
	{
		auto commentWidth = CommentHelpers::getCommentWidth(v);
		auto offset = CommentHelpers::getCommentOffset(v);

		w += jmax(0, commentWidth + offset.getX());
	}

	return Rectangle<int>(p, p.translated(w, h));
}



void Helpers::fixOverlapRecursive(ValueTree& node, UndoManager* um, bool sortProcessNodesFirst)
{
	jassert(node.getType() == PropertyIds::Node);
	auto bounds = getBounds(node, false);

	auto nodes = node.getChildWithName(PropertyIds::Nodes);

	auto minX = 30;

	minX += Helpers::ParameterMargin + ParameterWidth;

	auto minY = 10 + HeaderHeight;

	auto childMinX = minX;
	auto childMinY = minY;

	RectangleList<int> processBounds, cableBounds, childBounds;

	std::vector<ValueTree> processNodes, cableNodes, allNodes;
	std::vector<std::string> processNames, cableNames, allNames;

	auto vertical = shouldBeVertical(node);

	for(auto c: nodes)
	{
		auto nt = getHeaderTitle(c).toStdString();
		allNodes.push_back(c);
		allNames.push_back(nt);

		if(isProcessNode(c) || isContainerNode(c))
		{
			processNodes.push_back(c);
			processNames.push_back(nt);
		}
			
		else
		{
			cableNodes.push_back(c);
			cableNames.push_back(nt);
		}
	}

	if(!allNodes.empty())
	{
		auto arrangeList = [&](std::vector<ValueTree>& list, RectangleList<int>& listBounds)
		{
			for (auto& pn : list)
			{
				fixOverlapRecursive(pn, um, sortProcessNodesFirst);

				// include the comment box in the space calculations...
				auto cb = getBounds(pn, true);

				setMinPosition(cb, { childMinX, childMinY });

				if (listBounds.intersectsRectangle(cb))
				{
					auto fullBounds = listBounds.getBounds();

					if (vertical)
						cb = cb.withY(fullBounds.getBottom() + NodeMargin);
					else
						cb = cb.withX(fullBounds.getRight() + NodeMargin);
				}

				// now without comments
				auto realBounds = getBounds(pn, false);

				updateBounds(pn, realBounds.withPosition(cb.getPosition()), um);
				childBounds.addWithoutMerging(cb);
				listBounds.addWithoutMerging(cb);
			}
		};

		if(sortProcessNodesFirst)
		{
			if (vertical)
			{
				arrangeList(cableNodes, cableBounds);
				childMinX = childBounds.getBounds().getRight() + NodeMargin;
				arrangeList(processNodes, processBounds);
			}
			else
			{
				arrangeList(processNodes, processBounds);
				childMinY = childBounds.getBounds().getBottom() + NodeMargin;
				arrangeList(cableNodes, cableBounds);
			}
		}
		else
		{
			arrangeList(allNodes, processBounds);
		}
	}

	auto fullBounds = childBounds.getBounds();

	auto currentWidth = (int)node[UIPropertyIds::width];
	auto currentHeight = (int)node[UIPropertyIds::height];

	updateBounds(node, {
		jmax(bounds.getX(), minX),
		jmax(bounds.getY(), minY),
		jmax(currentWidth, fullBounds.getRight() + NodeMargin, bounds.getWidth()),
		jmax(currentHeight, fullBounds.getBottom() + NodeMargin, bounds.getHeight()),
		}, um);
}

void Helpers::updateChannelRecursive(ValueTree& v, int numChannels, UndoManager* um)
{
	jassert(v.getType() == PropertyIds::Node);

	auto path = getFactoryPath(v);

	auto isMulti = path.second == "multi";
	auto isSideChain = path.second == "sidechain";
	auto isModChain = path.second == "modchain";

	if (isSideChain)
		numChannels *= 2;

	if (isModChain)
		numChannels = 1;

	v.setProperty(PropertyIds::CompileChannelAmount, numChannels, um);

	auto childNodes = v.getChildWithName(PropertyIds::Nodes);

	if (isMulti)
		numChannels /= childNodes.getNumChildren();


	for (auto cn : childNodes)
	{
		updateChannelRecursive(cn, numChannels, um);
	}
}

void Helpers::resetLayout(ValueTree& nt, UndoManager* um)
{
	resetLayoutRecursive(nt, nt, um);
	fixOverlap(nt, um, true);
}

void Helpers::resetLayoutRecursive(ValueTree& root, ValueTree& child, UndoManager* um)
{
	jassert(child.getType() == PropertyIds::Node);

	if (child != root)
	{
		child.removeProperty(UIPropertyIds::x, um);
		child.removeProperty(UIPropertyIds::y, um);
	}

	if (isContainerNode(child) && (bool)child[UIPropertyIds::Locked])
		return;

	if (isContainerNode(child) && !(bool)child[PropertyIds::Folded])
	{
		child.removeProperty(UIPropertyIds::width, um);
		child.removeProperty(UIPropertyIds::height, um);
	}

	for(auto cn: child.getChildWithName(PropertyIds::Nodes))
	{
		resetLayoutRecursive(root, cn, um);
	}
}

juce::Path Helpers::createPinHole()
{
	Path pin;

	static const unsigned char pathData[] = { 110,109,78,128,38,68,28,209,44,67,108,0,128,69,68,172,104,116,67,108,0,128,69,68,200,203,193,67,108,78,128,38,68,114,151,229,67,108,0,128,7,68,200,203,193,67,108,0,128,7,68,172,104,116,67,108,78,128,38,68,28,209,44,67,99,109,78,128,38,68,228,3,107,67,
98,182,83,27,68,228,3,107,67,56,65,18,68,210,166,135,67,56,65,18,68,0,0,158,67,98,56,65,18,68,46,89,180,67,182,83,27,68,14,126,198,67,78,128,38,68,14,126,198,67,98,228,172,49,68,14,126,198,67,100,191,58,68,46,89,180,67,100,191,58,68,0,0,158,67,98,100,
191,58,68,210,166,135,67,228,172,49,68,228,3,107,67,78,128,38,68,228,3,107,67,99,109,78,128,38,68,40,142,132,67,98,250,133,45,68,40,142,132,67,58,57,51,68,228,244,143,67,58,57,51,68,0,0,158,67,98,58,57,51,68,26,11,172,67,250,133,45,68,216,113,183,67,
78,128,38,68,216,113,183,67,98,162,122,31,68,216,113,183,67,96,199,25,68,26,11,172,67,96,199,25,68,0,0,158,67,98,96,199,25,68,228,244,143,67,162,122,31,68,40,142,132,67,78,128,38,68,40,142,132,67,99,101,0,0 };

	pin.loadPathFromData(pathData, sizeof(pathData));
	return pin;
}

int CommentHelpers::getCommentWidth(ValueTree data)
{
	 return jmax(128, (int)data[UIPropertyIds::CommentWidth]);
}

Point<int> CommentHelpers::getCommentOffset(ValueTree data)
{
	return { (int)data[UIPropertyIds::CommentOffsetX], (int)data[UIPropertyIds::CommentOffsetY] };
}

void LayoutTools::alignVertically(const Array<ValueTree>& list, UndoManager* um)
{
	RectangleList<int> bounds;

	for(auto& l: list)
	{
		bounds.addWithoutMerging(Helpers::getBounds(l, true));
	}

	auto newX = bounds.getBounds().getX();

	for(auto l: list)
		l.setProperty(UIPropertyIds::x, newX, um);
}

void LayoutTools::alignHorizontally(const Array<ValueTree>& list, UndoManager* um)
{
	RectangleList<int> bounds;

	for (auto& l : list)
	{
		bounds.addWithoutMerging(Helpers::getBounds(l, true));
	}

	auto newY = bounds.getBounds().getY();

	for (auto l : list)
		l.setProperty(UIPropertyIds::y, newY, um);
}



void LayoutTools::distributeVertically(const Array<ValueTree>& list, UndoManager* um)
{
	

	distributeInternal(list, um, false);
}

void LayoutTools::distributeHorizontally(const Array<ValueTree>& list, UndoManager* um)
{
	distributeInternal(list, um, true);
}

void LayoutTools::distributeInternal(const Array<ValueTree>& list, UndoManager* um, bool horizontal)
{
	Array<ValueTree> sorted;

	if (list.size() < 2)
		return;

	RectangleList<int> bounds;

	if(horizontal)
	{
		Sorter<'x'> sorter;
		for (auto& l : list)
			sorted.addSorted(sorter, l);
		
	}
	else
	{
		Sorter<'y'> sorter;
		for (auto& l : list)
			sorted.addSorted(sorter, l);
	}
	
	for(auto& l: list)
		bounds.addWithoutMerging(Helpers::getBounds(l, true));

	auto fullBounds = bounds.getBounds();

	auto start = horizontal ? fullBounds.getX() : fullBounds.getY();
	auto end = horizontal ? fullBounds.getRight() : fullBounds.getBottom();

	auto fullSize = end - start;
	auto usedSize = 0;

	for (const auto& l : bounds)
		usedSize += horizontal ? l.getWidth() : l.getHeight();

	auto spaceLeft = fullSize - usedSize;
	auto gapSize = spaceLeft / (list.size() - 1);

	auto p = start;

	for (auto l : sorted)
	{
		auto pos = Helpers::getPosition(l);
		l.setProperty(horizontal ? UIPropertyIds::x : UIPropertyIds::y, p, um);
		p += (int)l[horizontal ? UIPropertyIds::width : UIPropertyIds::height];
		p += gapSize;
	}
}

}