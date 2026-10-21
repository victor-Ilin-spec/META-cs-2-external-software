#pragma once

namespace framework
{
    class c_text_input : public c_base_element
    {
    public:
        c_text_input(std::string label, std::string* var, bool hide_label = false);
        void draw() override;
        void input() override;

    private:
        struct char_entry {
            char  c;
            float birth;
            float death = 0.f;   // 0 = alive
            bool  dying = false;
        };

        std::string* m_var{};
        std::vector<char_entry> m_chars;   // source of truth for rendering
        float                   blink{};

        void rebuild_var();                // syncs m_chars → *m_var
    };
}