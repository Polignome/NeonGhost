#pragma once
#include "Rect.h"
#include "Texture.h" // ng::renderer::buffers::PixelBufferRGBA
#include <vector>
#include <memory>

namespace ng::lightmap
{
    // ------------------------------------------------------------
    // LightMapPartitionLeaf
    //
    // ushift/vshift/uscale/vscale entsprechen dem JS-Original
    // (normalisierte Atlas-UV-Koordinaten des Leafs, mit 1px
    // Sicherheitsrand). Werden aktuell nicht zwingend gebraucht, da
    // Lightmap.h die finalen UVs direkt aus lmPosX/Y/W/H berechnet -
    // bleiben aber als Bequemlichkeit erhalten.
    // ------------------------------------------------------------
    struct LightMapPartitionLeaf
    {
        Rect rect;
        float ushift, vshift, uscale, vscale;
        int id = -1;

        LightMapPartitionLeaf(const Rect& r, float us, float vs, float uw, float vh)
            : rect(r), ushift(us), vshift(vs), uscale(uw), vscale(vh) {
        }
    };

    class LightMapAtlas; // fwd

    // ------------------------------------------------------------
    // LightMapPartitionNode
    //
    // Guillotine-Binaerbaum-Packer, 1:1 nach dem JS-Original portiert.
    // Der Knoten, der schliesslich zum Leaf wird, besitzt dieses Leaf
    // selbst (unique_ptr) - Aufrufer bekommen nur einen rohen
    // Beobachter-Pointer zurueck, der so lange gueltig bleibt, wie der
    // Baum (also der LightMapAtlas) existiert.
    // ------------------------------------------------------------
    class LightMapPartitionNode
    {
    public:
        explicit LightMapPartitionNode(const Rect& r) : rect(r) {}

        // Sucht Platz fuer width x height. nullptr wenn nicht moeglich.
        LightMapPartitionLeaf* Build(int width, int height, LightMapAtlas& atlas)
        {
            if (node0 && node1)
            {
                if (auto* r = node0->Build(width, height, atlas)) return r;
                return node1->Build(width, height, atlas);
            }

            if (leaf) return nullptr;
            if (width > rect.width() || height > rect.height()) return nullptr;

            if (width == rect.width() && height == rect.height())
                return MakeLeaf(atlas);

            const int dw = rect.width() - width;
            const int dh = rect.height() - height;

            if (dw > dh)
            {
                node0 = std::make_unique<LightMapPartitionNode>(
                    Rect(rect.left, rect.top, rect.left + width, rect.bottom));
                node1 = std::make_unique<LightMapPartitionNode>(
                    Rect(rect.left + width, rect.top, rect.right, rect.bottom));
            }
            else
            {
                node0 = std::make_unique<LightMapPartitionNode>(
                    Rect(rect.left, rect.top, rect.right, rect.top + height));
                node1 = std::make_unique<LightMapPartitionNode>(
                    Rect(rect.left, rect.top + height, rect.right, rect.bottom));
            }

            return node0->Build(width, height, atlas);
        }

    private:
        Rect rect;
        std::unique_ptr<LightMapPartitionNode> node0, node1;
        std::unique_ptr<LightMapPartitionLeaf> leaf; // Owner

        LightMapPartitionLeaf* MakeLeaf(LightMapAtlas& atlas); // impl unten (braucht Atlas-Groesse)
    };

    // ------------------------------------------------------------
    // LightMapAtlas
    //
    // Wrappt einen PixelBufferRGBA plus den Packbaum. Ein Atlas wird
    // pro Bake-Versuch frisch angelegt (siehe BuildLightmapAtlas) -
    // bei Ueberlauf wird verworfen und ein groesserer Atlas neu gebaut,
    // genau wie im JS-Original (voller Rebuild pro CalcLightmap-Aufruf).
    // ------------------------------------------------------------
    class LightMapAtlas
    {
    public:
        LightMapAtlas(int width, int height)
            : m_width(width), m_height(height),
            m_buffer(width, height),
            m_root(std::make_unique<LightMapPartitionNode>(Rect(0, 0, width, height)))
        {
        }

        inline int Width()  const { return m_width; }
        inline int Height() const { return m_height; }

        inline ng::renderer::buffers::PixelBufferRGBA& Buffer() { return m_buffer; }
        inline const ng::renderer::buffers::PixelBufferRGBA& Buffer() const { return m_buffer; }

        // Reserviert ein Rechteck. nullptr = Atlas voll (Aufrufer muss
        // vergroessern und ALLES neu bauen, siehe BuildLightmapAtlas()).
        inline LightMapPartitionLeaf* Reserve(int w, int h)
        {
            return m_root->Build(w, h, *this);
        }

    private:
        int m_width, m_height;
        ng::renderer::buffers::PixelBufferRGBA m_buffer;
        std::unique_ptr<LightMapPartitionNode> m_root;
    };

    inline LightMapPartitionLeaf* LightMapPartitionNode::MakeLeaf(LightMapAtlas& atlas)
    {
        const int help_space = 1;

        const float u0 = float(rect.left + help_space) / float(atlas.Width() - help_space);
        const float v0 = float(rect.top + help_space) / float(atlas.Height() - help_space);
        const float u1 = float(rect.right - help_space) / float(atlas.Width() - help_space);
        const float v1 = float(rect.bottom - help_space) / float(atlas.Height() - help_space);

        leaf = std::make_unique<LightMapPartitionLeaf>(rect, u0, v0, u1 - u0, v1 - v0);
        return leaf.get();
    }
}