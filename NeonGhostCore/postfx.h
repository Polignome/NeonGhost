
#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <utility>
#include <stdexcept>

///////////////////////////////////////////////////////////////////////////
//                                                                       //
//   PostFX - Basissystem                                               //
//   ---------------------                                              //
//   Infrastruktur fuer PostFX-Effekte:                                 //
//                                                                       //
//   - GBuffer-Zugriff                                                   //
//   - PostFX-Interface                                                  //
//   - PostFXManager                                                     //
//   - Verwaltung und Ausfuehrung von Effekten                          //
//                                                                       //
///////////////////////////////////////////////////////////////////////////

namespace ng
{
    namespace renderer
    {
        namespace postfx
        {

            // -----------------------------------------------------------------
            // GBuffer
            //
            // Sammlung aller Buffer, die einem PostFX-Effekt zur Verfuegung
            // gestellt werden koennen.
            //
            // color ist immer der Arbeits-/Ausgabebuffer.
            // Die anderen Buffer sind normalerweise Read-Only.
            // -----------------------------------------------------------------
            template<typename T>
            struct GBuffer
            {
                uint32_t* color = nullptr;   // ARGB8888
                T* depth = nullptr;   // Z-Buffer
                uint32_t* material = nullptr;   // Material-/Shader-ID
                uint32_t* index = nullptr;   // Objekt-/Primitiv-Index
                uint32_t* normal = nullptr;   // gepackte Normale

                int width = 0;
                int height = 0;

                inline size_t PixelCount() const
                {
                    if (width <= 0 || height <= 0)
                        return 0;

                    return static_cast<size_t>(width) *
                        static_cast<size_t>(height);
                }

                inline bool HasColor() const
                {
                    return color != nullptr;
                }

                inline bool HasDepth() const
                {
                    return depth != nullptr;
                }

                inline bool HasMaterial() const
                {
                    return material != nullptr;
                }

                inline bool HasIndex() const
                {
                    return index != nullptr;
                }

                inline bool HasNormal() const
                {
                    return normal != nullptr;
                }
            };


            // -----------------------------------------------------------------
            // BufferSlot
            //
            // Gibt an, welche Buffer ein Effekt benoetigt.
            // Color ist implizit immer vorhanden.
            // -----------------------------------------------------------------
            enum class BufferSlot : uint8_t
            {
                Color,
                Depth,
                Material,
                Index,
                Normal
            };


            // -----------------------------------------------------------------
            // IPostFXEffect
            //
            // Basisklasse fuer alle PostFX-Effekte.
            // -----------------------------------------------------------------
            template<class T>
            class IPostFXEffect
            {
            public:

                virtual ~IPostFXEffect() = default;


                // -------------------------------------------------------------
                // Effekt ausfuehren
                // -------------------------------------------------------------
                virtual void Apply(GBuffer<T>& gbuffer, T time) = 0;


                // -------------------------------------------------------------
                // Benoetigte Buffer
                //
                // Color wird vom Manager IMMER zur Verfuegung gestellt.
                // -------------------------------------------------------------
                virtual std::vector<BufferSlot> GetRequiredInputs() const
                {
                    return { BufferSlot::Color };
                }


                // -------------------------------------------------------------
                // Name fuer Debug/UI/Logging
                // -------------------------------------------------------------
                virtual const char* GetName() const = 0;


                // -------------------------------------------------------------
                // Effekt aktiv/inaktiv
                // -------------------------------------------------------------
                bool enabled = true;
            };


            // -----------------------------------------------------------------
            // PostFXManager
            // -----------------------------------------------------------------
            template<class T>
            class PostFXManager
            {
            public:

                // =============================================================
                // Input-Verwaltung
                // =============================================================

                void SetSize(int width, int height)
                {
                    m_width = std::max(0, width);
                    m_height = std::max(0, height);
                }


                void BindColor(uint32_t* ptr)
                {
                    m_color = ptr;
                }


                void BindDepth(T* ptr)
                {
                    m_depth = ptr;
                }


                void BindMaterial(uint32_t* ptr)
                {
                    m_material = ptr;
                }


                void BindIndex(uint32_t* ptr)
                {
                    m_index = ptr;
                }


                void BindNormal(uint32_t* ptr)
                {
                    m_normal = ptr;
                }


                // =============================================================
                // GBuffer fuer einen Effekt aufbauen
                // =============================================================
                GBuffer<T> ResolveInputsFor(
                    const IPostFXEffect<T>& effect) const
                {
                    GBuffer<T> gb;

                    gb.width = m_width;
                    gb.height = m_height;

                    // ---------------------------------------------------------
                    // Color ist IMMER vorhanden.
                    // Jeder PostFX-Effekt darf seinen Output hierhin schreiben.
                    // ---------------------------------------------------------
                    gb.color = m_color;

                    // ---------------------------------------------------------
                    // Zusaetzliche Inputs nur dann binden, wenn der Effekt sie
                    // explizit angefordert hat.
                    // ---------------------------------------------------------
                    for (BufferSlot slot : effect.GetRequiredInputs())
                    {
                        switch (slot)
                        {
                        case BufferSlot::Color:
                            // Bereits oben gebunden.
                            break;

                        case BufferSlot::Depth:
                            gb.depth = m_depth;
                            break;

                        case BufferSlot::Material:
                            gb.material = m_material;
                            break;

                        case BufferSlot::Index:
                            gb.index = m_index;
                            break;

                        case BufferSlot::Normal:
                            gb.normal = m_normal;
                            break;

                        default:
                            break;
                        }
                    }

                    return gb;
                }


                // =============================================================
                // Pruefen, ob alle benoetigten Buffer vorhanden sind
                // =============================================================
                bool IsSatisfied(
                    const IPostFXEffect<T>& effect) const
                {
                    // ---------------------------------------------------------
                    // Color ist fuer jeden Effekt Pflicht.
                    // ---------------------------------------------------------
                    if (m_color == nullptr)
                        return false;

                    for (BufferSlot slot : effect.GetRequiredInputs())
                    {
                        switch (slot)
                        {
                        case BufferSlot::Color:
                            if (m_color == nullptr)
                                return false;
                            break;

                        case BufferSlot::Depth:
                            if (m_depth == nullptr)
                                return false;
                            break;

                        case BufferSlot::Material:
                            if (m_material == nullptr)
                                return false;
                            break;

                        case BufferSlot::Index:
                            if (m_index == nullptr)
                                return false;
                            break;

                        case BufferSlot::Normal:
                            if (m_normal == nullptr)
                                return false;
                            break;

                        default:
                            break;
                        }
                    }

                    return true;
                }


                // =============================================================
                // Effekt hinzufuegen
                //
                // Uebernimmt den Besitz des unique_ptr.
                // =============================================================
                IPostFXEffect<T>* Add(
                    std::unique_ptr<IPostFXEffect<T>> effect)
                {
                    if (!effect)
                        return nullptr;

                    IPostFXEffect<T>* raw = effect.get();

                    m_effects.push_back(std::move(effect));

                    return raw;
                }


                // =============================================================
                // Effekt direkt konstruieren
                //
                // Wichtig:
                // Hier NICHT erneut "T" als Template-Parameter verwenden,
                // da T bereits der Datentyp des PostFXManager ist.
                // =============================================================
                template<typename EffectT, typename... Args>
                EffectT* AddEffect(Args&&... args)
                {
                    static_assert(
                        std::is_base_of<
                        IPostFXEffect<T>,
                        EffectT
                        >::value,
                        "EffectT must derive from IPostFXEffect<T>."
                        );

                    auto effect =
                        std::make_unique<EffectT>(
                            std::forward<Args>(args)...
                        );

                    EffectT* raw = effect.get();

                    m_effects.push_back(std::move(effect));

                    return raw;
                }


                // =============================================================
                // Alle Effekte entfernen
                // =============================================================
                void Clear()
                {
                    m_effects.clear();
                }


                // =============================================================
                // Anzahl der Effekte
                // =============================================================
                size_t Count() const
                {
                    return m_effects.size();
                }


                // =============================================================
                // Effekt holen
                // =============================================================
                IPostFXEffect<T>* Get(size_t i)
                {
                    if (i >= m_effects.size())
                        return nullptr;

                    return m_effects[i].get();
                }


                // Const-Version
                // =============================================================
                const IPostFXEffect<T>* Get(size_t i) const
                {
                    if (i >= m_effects.size())
                        return nullptr;

                    return m_effects[i].get();
                }


                // =============================================================
                // Effekt per Name suchen
                // =============================================================
                IPostFXEffect<T>* Find(const char* name)
                {
                    if (name == nullptr)
                        return nullptr;

                    for (auto& fx : m_effects)
                    {
                        if (!fx)
                            continue;

                        const char* fxName = fx->GetName();

                        if (fxName &&
                            std::strcmp(fxName, name) == 0)
                        {
                            return fx.get();
                        }
                    }

                    return nullptr;
                }


                // Const-Version
                // =============================================================
                const IPostFXEffect<T>* Find(const char* name) const
                {
                    if (name == nullptr)
                        return nullptr;

                    for (const auto& fx : m_effects)
                    {
                        if (!fx)
                            continue;

                        const char* fxName = fx->GetName();

                        if (fxName &&
                            std::strcmp(fxName, name) == 0)
                        {
                            return fx.get();
                        }
                    }

                    return nullptr;
                }


                // =============================================================
                // PostFX-Stack ausfuehren
                // =============================================================
                void Apply(
                    T time,
                    bool warnOnMissingInputs = true)
                {
                    for (auto& fx : m_effects)
                    {
                        if (!fx)
                            continue;

                        // -----------------------------------------------------
                        // Deaktivierter Effekt
                        // -----------------------------------------------------
                        if (!fx->enabled)
                            continue;


                        // -----------------------------------------------------
                        // Benoetigte Buffer vorhanden?
                        // -----------------------------------------------------
                        if (!IsSatisfied(*fx))
                        {
                            if (warnOnMissingInputs)
                            {
                                const char* name = fx->GetName();

                                if (!name)
                                    name = "<unnamed>";

                                std::fprintf(
                                    stderr,
                                    "[PostFXManager] Effekt '%s' "
                                    "uebersprungen: benoetigter "
                                    "Input-Buffer nicht gebunden.\n",
                                    name
                                );
                            }

                            continue;
                        }


                        // -----------------------------------------------------
                        // Individuellen GBuffer fuer diesen Effekt erzeugen
                        // -----------------------------------------------------
                        GBuffer<T> view = ResolveInputsFor(*fx);


                        // -----------------------------------------------------
                        // Effekt ausfuehren
                        // -----------------------------------------------------
                        fx->Apply(view, time);
                    }
                }


                // =============================================================
                // Einzelnen Effekt entfernen
                // =============================================================
                bool Remove(size_t index)
                {
                    if (index >= m_effects.size())
                        return false;

                    m_effects.erase(m_effects.begin() + index);

                    return true;
                }


                // =============================================================
                // Zugriff auf den internen Stack
                // =============================================================
                std::vector<std::unique_ptr<IPostFXEffect<T>>>& Effects()
                {
                    return m_effects;
                }


                const std::vector<
                    std::unique_ptr<IPostFXEffect<T>>>& Effects() const
                {
                    return m_effects;
                }


            private:

                // =============================================================
                // Effekt-Stack
                // =============================================================
                std::vector<
                    std::unique_ptr<IPostFXEffect<T>>
                > m_effects;


                // =============================================================
                // Gebundene Renderbuffer
                // =============================================================
                uint32_t* m_color = nullptr;
                T* m_depth = nullptr;
                uint32_t* m_material = nullptr;
                uint32_t* m_index = nullptr;
                uint32_t* m_normal = nullptr;


                // =============================================================
                // Aufloesung
                // =============================================================
                int m_width = 0;
                int m_height = 0;
            };

        } // namespace postfx
    } // namespace renderer
} // namespace ng

