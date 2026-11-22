#include "ui/EchoVisualizer.h"

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"

namespace oe::ui
{

namespace
{
    constexpr int kMaxEchoesPerChannel = 48;
    constexpr float kMinLevel = 0.03f;
} // namespace

EchoVisualizer::EchoVisualizer (OrbitEchoProcessor& p) : processor (p)
{
    echoes.reserve (static_cast<std::size_t> (kMaxEchoesPerChannel * 8));
    setOpaque (true);
    lastTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

EchoVisualizer::~EchoVisualizer() = default;

void EchoVisualizer::buildEchoes (const VisualizerFrame& f, std::vector<Echo>& out, float window)
{
    out.clear();
    const auto fb = juce::jlimit (0.0f, 1.0f, f.feedback);
    auto push = [&out, window] (float t, float level, int ch)
    {
        if (t >= 0.0f && t <= window && level >= kMinLevel)
            out.push_back ({ t, juce::jmin (1.0f, level), ch });
    };

    const auto dl = juce::jmax (0.5f, f.tapMs[0]);
    const auto dr = juce::jmax (0.5f, f.tapMs[1]);

    switch (f.style)
    {
        case 3: // PingPong: L, R, L, R ...
        {
            auto t = dl;
            auto level = 1.0f;
            for (int k = 0; k < 2 * kMaxEchoesPerChannel && t <= window && level >= kMinLevel; ++k)
            {
                push (t, level, k % 2);
                t += (k % 2 == 0) ? dr : dl;
                level *= fb;
            }
            break;
        }
        case 4: // Quad: first pass from every active head, then recirculation every D
        {
            int active = 0;
            for (const auto on : f.headsOn)
                active += on ? 1 : 0;
            const auto norm = active > 0 ? 1.0f / std::sqrt (static_cast<float> (active)) : 0.0f;
            const auto period = juce::jmax (0.5f, f.tapMs[6]);
            for (int h = 0; h < 4; ++h)
            {
                if (! f.headsOn[static_cast<std::size_t> (h)])
                    continue;
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto level = norm * 1.6f;
                    const auto t0 = f.tapMs[static_cast<std::size_t> (2 * h + ch)];
                    for (int k = 0; k < kMaxEchoesPerChannel; ++k)
                    {
                        push (t0 + static_cast<float> (k) * period, level, ch);
                        level *= fb;
                        if (level < kMinLevel)
                            break;
                    }
                }
            }
            break;
        }
        default: // Single, Dual, Ratio: each side repeats at its own time
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                const auto d = ch == 0 ? dl : dr;
                auto level = 1.0f;
                for (int k = 1; k <= kMaxEchoesPerChannel; ++k)
                {
                    push (static_cast<float> (k) * d, level, ch);
                    level *= fb;
                    if (level < kMinLevel)
                        break;
                }
            }
            break;
        }
    }
}

void EchoVisualizer::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto dt = static_cast<float> (juce::jlimit (0.0, 100.0, now - lastTime));
    lastTime = now;

    VisualizerFrame newest;
    if (processor.popVisualizerFrame (newest))
        frame = newest;

    // Window: about 2.4 times the longest head (so a few repeats are visible), 0.5 s minimum.
    float longest = 0.0f;
    for (int t = 0; t < frame.numTaps; ++t)
        longest = juce::jmax (longest, frame.tapMs[static_cast<std::size_t> (t)]);
    const auto target = juce::jlimit (500.0f, 48000.0f, longest * (frame.style == 3 ? 4.5f : 2.6f));
    windowMs += 0.15f * (target - windowMs);

    buildEchoes (frame, echoes, windowMs);

    sweepMs += dt;
    if (sweepMs > windowMs)
        sweepMs = 0.0f;
    glow += 0.2f * (juce::jlimit (0.0f, 1.0f, frame.wetRms * 6.0f) - glow);

    repaint();
}

void EchoVisualizer::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::screen);
    g.fillRoundedRectangle (r, 6.0f);

    auto plot = r.reduced (12.0f, 10.0f);
    const auto mid = plot.getCentreY();
    const auto halfH = plot.getHeight() * 0.5f - 4.0f;
    auto xFor = [&plot, this] (float ms) { return plot.getX() + plot.getWidth() * ms / windowMs; };

    // Grid: lines at a "nice" step, labelled.
    const float steps[] { 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    float step = steps[0];
    for (const auto s : steps)
        if (windowMs / s <= 8.0f)
        {
            step = s;
            break;
        }
    auto* lf = dynamic_cast<OrbitLookAndFeel*> (&getLookAndFeel());
    g.setFont (lf != nullptr ? lf->font (10.0f) : juce::Font (juce::FontOptions (10.0f)));
    for (float t = step; t < windowMs; t += step)
    {
        const auto x = xFor (t);
        g.setColour (colours::panelEdge.withAlpha (0.55f));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        g.setColour (colours::textDim.withAlpha (0.7f));
        const auto label = t >= 1000.0f ? juce::String (t / 1000.0f, t >= 10000.0f ? 0 : 1) + " s" : juce::String (juce::roundToInt (t)) + " ms";
        g.drawText (label, juce::Rectangle<float> (x + 3.0f, plot.getY() - 2.0f, 60.0f, 12.0f), juce::Justification::left);
    }
    g.setColour (colours::panelEdge);
    g.drawHorizontalLine (juce::roundToInt (mid), plot.getX(), plot.getRight());
    g.setColour (colours::textDim.withAlpha (0.6f));
    g.drawText ("L", juce::Rectangle<float> (r.getX() + 2.0f, plot.getY(), 10.0f, 12.0f), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (r.getX() + 2.0f, plot.getBottom() - 12.0f, 10.0f, 12.0f), juce::Justification::centred);

    // Dry hit at t = 0.
    g.setColour (colours::text.withAlpha (0.6f));
    g.fillRect (juce::Rectangle<float> (plot.getX() - 1.0f, mid - halfH * 0.95f, 2.0f, halfH * 1.9f));

    // Echoes: blurred bars (Diffusion widens and softens them).
    const auto blur = frame.diffusion;
    for (const auto& e : echoes)
    {
        const auto x = xFor (e.timeMs);
        const auto h = halfH * std::sqrt (e.level);
        const auto near = 1.0f - juce::jlimit (0.0f, 1.0f, std::abs (e.timeMs - sweepMs) / (0.04f * windowMs + 1.0f));
        const auto colour = colours::amber.interpolatedWith (colours::copper, 0.35f * (1.0f - e.level));
        const auto alpha = 0.35f + 0.45f * e.level + 0.2f * near + 0.15f * glow;
        const auto top = e.channel == 0 ? mid - h : mid;
        const auto bar = juce::Rectangle<float> (x - 1.5f, top, 3.0f, h);

        if (blur > 0.01f)
        {
            const auto spread = 2.0f + blur * juce::jmin (36.0f, plot.getWidth() * 0.035f);
            for (int layer = 3; layer >= 1; --layer)
            {
                const auto w = spread * static_cast<float> (layer) / 3.0f;
                g.setColour (colour.withAlpha (alpha * 0.18f * static_cast<float> (4 - layer)));
                g.fillRoundedRectangle (bar.withSizeKeepingCentre (w * 2.0f, h).withY (top), w * 0.5f);
            }
        }
        g.setColour (colour.withAlpha (juce::jmin (1.0f, alpha * (1.0f - 0.6f * blur))));
        g.fillRoundedRectangle (bar, 1.5f);
        if (near > 0.0f)
        {
            g.setColour (colours::amber.withAlpha (0.25f * near));
            g.fillRoundedRectangle (bar.expanded (3.0f, 0.0f), 3.0f);
        }
    }

    // Sweeping read head.
    const auto sx = xFor (sweepMs);
    g.setGradientFill (juce::ColourGradient (colours::amber.withAlpha (0.0f), sx - 24.0f, mid, colours::amber.withAlpha (0.18f), sx, mid, false));
    g.fillRect (juce::Rectangle<float> (sx - 24.0f, plot.getY(), 24.0f, plot.getHeight()));
    g.setColour (colours::amber.withAlpha (0.55f));
    g.drawVerticalLine (juce::roundToInt (sx), plot.getY(), plot.getBottom());

    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

} // namespace oe::ui
