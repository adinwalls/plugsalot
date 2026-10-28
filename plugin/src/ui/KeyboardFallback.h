// Plugsalot - GPL-3.0-or-later
// Makes text boxes typeable inside hosts that keep the system keyboard focus.
//
// JUCE only gives a text box the keyboard if the plugin's own window holds the
// operating system's input focus. Hosts that embed plugin windows (Ardour on
// Linux, for one) keep that focus on their own window and forward keystrokes
// to the plugin instead. Those keystrokes then reach JUCE with no focused
// component and are dropped. This helper remembers the text box the user last
// clicked and hands such keystrokes to it.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace plugsalot
{

class KeyboardFallback final : private juce::MouseListener, private juce::KeyListener
{
public:
    explicit KeyboardFallback(juce::Component& editorRoot) : root(editorRoot)
    {
        root.addMouseListener(this, true);
        root.addKeyListener(this);
    }

    ~KeyboardFallback() override
    {
        detachFromTopLevel();
        root.removeKeyListener(this);
        root.removeMouseListener(this);
    }

    // Call when the editor is attached to (or moved within) the host window: keys
    // with no focused component are delivered to the window's top-level component.
    void attachToTopLevel()
    {
        auto* top = root.getTopLevelComponent();
        if (top == topLevel.getComponent())
            return;
        detachFromTopLevel();
        topLevel = top;
        if (top != nullptr && top != &root)
            top->addKeyListener(this);
    }

    juce::TextEditor* currentTarget() const { return target.getComponent(); }

private:
    void detachFromTopLevel()
    {
        if (auto* t = topLevel.getComponent())
            if (t != &root)
                t->removeKeyListener(this);
        topLevel = nullptr;
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        attachToTopLevel();
        // A click in a text box (or on its scroll area) makes it the target; a
        // click anywhere else clears it so keys go back to the host.
        auto* c = e.eventComponent;
        juce::TextEditor* editor = nullptr;
        while (c != nullptr && c != &root && editor == nullptr)
        {
            editor = dynamic_cast<juce::TextEditor*>(c);
            c = c->getParentComponent();
        }
        setTarget(editor != nullptr && ! editor->isReadOnly() ? editor : nullptr);
    }

    void setTarget(juce::TextEditor* editor)
    {
        if (auto* old = target.getComponent())
            old->repaint();
        target = editor;
        if (editor != nullptr)
            editor->repaint();
    }

    bool keyPressed(const juce::KeyPress& key, juce::Component*) override
    {
        auto* t = target.getComponent();
        if (t == nullptr || ! t->isShowing() || ! t->isEnabled())
            return false;
        // Only step in when JUCE found no focused component of ours for the key.
        auto* focused = juce::Component::getCurrentlyFocusedComponent();
        if (focused == t)
            return false;
        if (focused != nullptr && root.isParentOf(focused))
            return false;
        return t->keyPressed(key);
    }

    juce::Component& root;
    juce::Component::SafePointer<juce::Component> topLevel;
    juce::Component::SafePointer<juce::TextEditor> target;
};

} // namespace plugsalot
