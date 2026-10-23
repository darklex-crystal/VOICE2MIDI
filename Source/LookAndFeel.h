#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vocal2midi
{
class TuneLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    TuneLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff0c1118));
        setColour (juce::Label::textColourId, juce::Colour (0xffdce6f2));
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffdce6f2));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff111923));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff273446));
        setColour (juce::Slider::thumbColourId, juce::Colour (0xff68e0c2));
        setColour (juce::Slider::trackColourId, juce::Colour (0xff273446));
        setColour (juce::Slider::backgroundColourId, juce::Colour (0xff111923));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff111923));
        setColour (juce::ComboBox::textColourId, juce::Colour (0xffdce6f2));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff273446));
        setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff8fa4ba));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff151e29));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xffdce6f2));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff24594f));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1a2a35));
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xffdce6f2));
        setColour (juce::ToggleButton::textColourId, juce::Colour (0xffdce6f2));
        setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff68e0c2));
        setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff4a5b6c));
        setColour (juce::GroupComponent::outlineColourId, juce::Colour (0xff253142));
        setColour (juce::GroupComponent::textColourId, juce::Colour (0xff8fa4ba));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                                    minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const auto track = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y) + height * 0.5f - 2.0f,
                                                   static_cast<float> (width), 4.0f);
        g.setColour (findColour (juce::Slider::trackColourId));
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (juce::Colour (0xff68e0c2));
        g.fillRoundedRectangle (track.withWidth (std::max (4.0f, sliderPos - track.getX())), 2.0f);
        g.fillEllipse (sliderPos - 5.0f, track.getCentreY() - 5.0f, 10.0f, 10.0f);
    }
};
} // namespace vocal2midi
