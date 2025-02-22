/***************************************************************************
 * sprite.cpp  -  basic sprite class
 *
 * Copyright © 2003 - 2011 Florian Richter
 * Copyright © 2012-2020 The TSC Contributors
 ***************************************************************************/
/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../objects/sprite.hpp"
#include "../objects/movingsprite.hpp"
#include "../core/game_core.hpp"
#include "../level/level.hpp"
#include "../core/framerate.hpp"
#include "../level/level_player.hpp"
#include "../gui/hud.hpp"
#include "../video/gl_surface.hpp"
#include "../video/renderer.hpp"
#include "../core/sprite_manager.hpp"
#include "../core/editor/editor.hpp"
#include "../core/i18n.hpp"
#include "../scripting/events/touch_event.hpp"
#include "../level/level_settings.hpp"
#include "../level/level_editor.hpp"
#include "../overworld/world_editor.hpp"
#include "../core/file_parser.hpp"
#include "../core/filesystem/resource_manager.hpp"
#include "../core/xml_attributes.hpp"
#include "../core/global_basic.hpp"
#include "../user/savegame/savegame.hpp"

using namespace std;

namespace fs = boost::filesystem;

namespace TSC {

/* *** *** *** *** *** *** *** *** cCollidingSprite *** *** *** *** *** *** *** *** *** */

cCollidingSprite::cCollidingSprite(cSprite_Manager* sprite_manager)
{
    m_sprite_manager = sprite_manager;
}

cCollidingSprite::~cCollidingSprite(void)
{
    Clear_Collisions();
}

void cCollidingSprite::Set_Sprite_Manager(cSprite_Manager* sprite_manager)
{
    m_sprite_manager = sprite_manager;
}

void cCollidingSprite::Handle_Collisions(void)
{
    // get collision list
    cObjectCollision_List col_list;
    m_collisions.swap(col_list);

    // parse the given collisions
    for (cObjectCollision_List::iterator itr = col_list.begin(); itr != col_list.end(); ++itr) {
        // handle it
        Handle_Collision((*itr));
    }

    // clear
    for (cObjectCollision_List::iterator itr = col_list.begin(); itr != col_list.end(); ++itr) {
        delete *itr;
    }

    col_list.clear();
}

cObjectCollision* cCollidingSprite::Create_Collision_Object(const cSprite* base, cSprite* col, Col_Valid_Type valid_type) const
{
    // if invalid
    if (!base || valid_type == COL_VTYPE_NOT_VALID) {
        return NULL;
    }

    // create
    cObjectCollision* collision = new cObjectCollision();

    // if col object is available
    if (col) {
        // object
        collision->m_obj = col;
        // identifier
        if (col->m_sprite_array != ARRAY_PLAYER) {
            collision->m_number = m_sprite_manager->Get_Array_Num(col);
        }
        // type
        collision->m_array = col->m_sprite_array;
        // direction
        collision->Set_Direction(base, col);
    }

    // valid type
    collision->m_valid_type = valid_type;

    return collision;
}

bool cCollidingSprite::Add_Collision(cObjectCollision* collision, bool add_if_new /* = 0 */)
{
    // invalid collision data
    if (!collision) {
        return 0;
    }

    // check if collision data is new
    if (add_if_new) {
        // already in list
        if (Is_Collision_Included(collision->m_obj)) {
            delete collision;
            return 0;
        }
    }

    m_collisions.push_back(collision);

    return 1;
}

void cCollidingSprite::Add_Collisions(cObjectCollisionType* col_list, bool add_if_new /* = 0 */)
{
    // insert all objects
    for (cObjectCollision_List::iterator itr = col_list->objects.begin(); itr != col_list->objects.end(); ++itr) {
        cObjectCollision* col = (*itr);

        Add_Collision(col, add_if_new);
    }

    col_list->objects.clear();
}

void cCollidingSprite::Delete_Collision(cObjectCollision* collision)
{
    if (!collision) {
        return;
    }

    // get iterator
    cObjectCollision_List::iterator itr = std::find(m_collisions.begin(), m_collisions.end(), collision);

    // not available
    if (itr == m_collisions.end()) {
        return;
    }

    // erase from list
    m_collisions.erase(itr);
    // delete
    delete collision;
}

void cCollidingSprite::Delete_Last_Collision(void)
{
    if (m_collisions.empty()) {
        return;
    }

    cObjectCollision_List::iterator end_itr = m_collisions.end() - 1;

    delete *end_itr;
    m_collisions.erase(end_itr);
}

int cCollidingSprite::Is_Collision_In_Direction(const ObjectDirection dir) const
{
    int pos = 0;

    for (cObjectCollision_List::const_iterator itr = m_collisions.begin(); itr != m_collisions.end(); ++itr) {
        cObjectCollision* col = (*itr);

        if (col->m_direction == dir) {
            return pos;
        }

        pos++;
    }

    return -1;
}

cObjectCollision* cCollidingSprite::Get_Last_Collision(bool only_blocking /* = 0 */) const
{
    // no collisions available
    if (m_collisions.empty()) {
        return NULL;
    }

    if (only_blocking) {
        for (cObjectCollision_List::const_reverse_iterator itr = m_collisions.rbegin(); itr != m_collisions.rend(); ++itr) {
            // get object pointer
            cObjectCollision* col = (*itr);

            cSprite* col_obj = m_sprite_manager->Get_Pointer(col->m_number);

            // ignore passive
            if (col_obj->m_massive_type == MASS_PASSIVE) {
                continue;
            }
            // if active check if not climbable
            if (col->m_array == ARRAY_ACTIVE) {
                // not a valid object
                if (col_obj->m_massive_type == MASS_CLIMBABLE) {
                    continue;
                }
            }

            // is blocking
            return col;
        }

        // not found
        return NULL;
    }

    return *(m_collisions.end() - 1);
}

bool cCollidingSprite::Is_Collision_Included(const cSprite* obj) const
{
    // check if in collisions list
    for (cObjectCollision_List::const_iterator itr = m_collisions.begin(); itr != m_collisions.end(); ++itr) {
        // get object pointer
        cObjectCollision* col = (*itr);

        // is in list
        if (col->m_obj == obj) {
            return 1;
        }
    }

    // not found
    return 0;
}

void cCollidingSprite::Clear_Collisions(void)
{
    for (cObjectCollision_List::iterator itr = m_collisions.begin(); itr != m_collisions.end(); ++itr) {
        delete *itr;
    }

    m_collisions.clear();
}

void cCollidingSprite::Handle_Collision(cObjectCollision* collision)
{
    /* Issue the touch event, but only if we’re currently in
     * a level (remember: Sprites are not only used in levels...)
     * and that level is not being edited at the moment.
     * Also needed as the MRuby interpreter is not initialized before
     * Level construction. */
    if (pActive_Level) {
        if (!pLevel_Editor->m_enabled) {
            /* For whatever reason, CollidingSprite is the superclass
             * of Sprite (I’d expect it the other way round), so we have
             * first to check whether we got a correct sprite object prior
             * to handing it to the event (=> downcast). As all level-relevant
             * sprites are real Sprites (rather than bare CollidingSprites),
             * this doesn’t exclude important sprites from being listened to
             * in MRuby. --Marvin Gülker (aka Quintus) */
            cSprite* p_sprite = dynamic_cast<cSprite*>(this);
            if (p_sprite) {
                // Fire the event for both objects, eases registering
                Scripting::cTouch_Event evt1(collision->m_obj);
                Scripting::cTouch_Event evt2(p_sprite);
                evt1.Fire(pActive_Level->m_mruby, p_sprite);
                evt2.Fire(pActive_Level->m_mruby, collision->m_obj);
            }
        }
    }

    // player
    if (collision->m_array == ARRAY_PLAYER) {
        Handle_Collision_Player(collision);
    }
    // enemy
    else if (collision->m_array == ARRAY_ENEMY) {
        Handle_Collision_Enemy(collision);
    }
    // massive
    else if (collision->m_array == ARRAY_MASSIVE || collision->m_array == ARRAY_ACTIVE) {
        Handle_Collision_Massive(collision);
    }
    // passive
    else if (collision->m_array == ARRAY_PASSIVE) {
        Handle_Collision_Passive(collision);
    }
    // Lava
    else if (collision->m_array == ARRAY_LAVA) {
        Handle_Collision_Lava(collision);
    }
}

/* *** *** *** *** *** *** *** cSprite *** *** *** *** *** *** *** *** *** *** */

const float cSprite::m_pos_z_passive_start = 0.01f;
const float cSprite::m_pos_z_massive_start = 0.08f;
const float cSprite::m_pos_z_front_passive_start = 0.1f;
const float cSprite::m_pos_z_halfmassive_start = 0.04f;
const float cSprite::m_pos_z_player = 0.0999f;
const float cSprite::m_pos_z_delta = 0.000001f;

cSprite::cSprite(cSprite_Manager* sprite_manager, const std::string type_name /* = "sprite" */)
    : cCollidingSprite(sprite_manager), m_type_name(type_name)
{
    cSprite::Init();
}

cSprite::cSprite(XmlAttributes& attributes, cSprite_Manager* sprite_manager, const std::string type_name /* = "sprite" */)
    : cCollidingSprite(sprite_manager), m_type_name(type_name)
{
    cSprite::Init();

    // position
    Set_Pos(string_to_float(attributes["posx"]), string_to_float(attributes["posy"]), true);
    // image
    m_image_filename = attributes["image"];
    if(utf8_to_path(m_image_filename).extension() == utf8_to_path(".png")) {
        Set_Image(pVideo->Get_Surface(utf8_to_path(attributes["image"])), true) ;
    }
    else {
        if (Add_Image_Set("main", utf8_to_path(m_image_filename)))
            Set_Image_Set("main", true);
        else { // level XML points to invalid file
            std::cerr << "Warning: Level XML is invalid -- file does not load: " << m_image_filename << std::endl;
            m_image_filename = "game/image_not_found.png";
            Set_Image(pVideo->Get_Surface(utf8_to_path(m_image_filename)));
        }
    }
    // Massivity.
    // FIXME: Should be separate "massivity" attribute or so.
    Set_Massive_Type(Get_Massive_Type_Id(attributes["type"]));
}

cSprite::~cSprite(void)
{
    if (m_delete_image && m_image) {
        delete m_image;
        m_image = NULL;
    }
}

void cSprite::Init(void)
{
    // undefined
    m_type = TYPE_UNDEFINED;
    m_sprite_array = ARRAY_UNDEFINED;

    // animation data
    m_curr_img = -1;
    m_anim_enabled = 0;
    m_anim_img_start = 0;
    m_anim_img_end = 0;
    m_anim_time_default = 1000;
    m_anim_counter = 0;
    m_anim_last_ticks = pFramerate->m_last_ticks - 1;
    m_anim_mod = 1.0f;

    // collision data
    m_col_pos.m_x = 0.0f;
    m_col_pos.m_y = 0.0f;
    m_col_rect.m_x = 0.0f;
    m_col_rect.m_y = 0.0f;
    m_col_rect.m_w = 0.0f;
    m_col_rect.m_h = 0.0f;
    // image data
    m_rect.m_x = 0.0f;
    m_rect.m_y = 0.0f;
    m_rect.m_w = 0.0f;
    m_rect.m_h = 0.0f;
    m_start_rect.clear();

    m_start_pos_x = 0.0f;
    m_start_pos_y = 0.0f;

    m_start_image = NULL;
    m_image = NULL;
    m_auto_destroy = 0;
    m_delete_image = 0;
    m_shadow_pos = 0.0f;
    m_shadow_color = black;
    m_no_camera = 0;

    m_color = static_cast<uint8_t>(255);

    m_combine_type = 0;
    m_combine_color[0] = 0.0f;
    m_combine_color[1] = 0.0f;
    m_combine_color[2] = 0.0f;

    m_pos_x = 0.0f;
    m_pos_y = 0.0f;
    m_pos_z = 0.0f;
    m_editor_pos_z = 0.0f;

    m_massive_type = MASS_PASSIVE;
    m_active = 1;
    m_spawned = 0;
    m_suppress_save = 0;
    m_camera_range = 1000;
    m_can_be_ground = 0;
    m_disallow_managed_delete = 0;

    // rotation
    m_rotation_affects_rect = 0;
    m_start_rot_x = 0.0f;
    m_start_rot_y = 0.0f;
    m_start_rot_z = 0.0f;
    m_rot_x = 0.0f;
    m_rot_y = 0.0f;
    m_rot_z = 0.0f;
    // scale
    m_scale_affects_rect = 0;
    m_scale_up = 0;
    m_scale_down = 1;
    m_scale_left = 0;
    m_scale_right = 1;
    m_start_scale_x = 1.0f;
    m_start_scale_y = 1.0f;
    m_scale_x = 1.0f;
    m_scale_y = 1.0f;

    m_valid_draw = 1;
    m_valid_update = 1;

    m_uid = -1;
}

cSprite* cSprite::Copy(void) const
{
    cSprite* basic_sprite = new cSprite(m_sprite_manager);
 
    // current image
    basic_sprite->m_image_filename = m_image_filename;
    basic_sprite->Set_Image(m_start_image, true);

    // animation details
    basic_sprite->m_curr_img = m_curr_img;
    basic_sprite->m_anim_enabled = m_anim_enabled;
    basic_sprite->m_anim_img_start = m_anim_img_start;
    basic_sprite->m_anim_img_end = m_anim_img_end;
    basic_sprite->m_anim_time_default = m_anim_time_default;
    basic_sprite->m_anim_counter = m_anim_counter;
    basic_sprite->m_anim_last_ticks = m_anim_last_ticks;
    basic_sprite->m_anim_mod = m_anim_mod;
    basic_sprite->m_images = m_images;
    basic_sprite->m_named_ranges = m_named_ranges;

    // basic settings
    basic_sprite->Set_Pos(m_start_pos_x, m_start_pos_y, 1);
    basic_sprite->m_type = m_type;
    basic_sprite->m_sprite_array = m_sprite_array;
    basic_sprite->Set_Massive_Type(m_massive_type);
    basic_sprite->m_can_be_ground = m_can_be_ground;
    basic_sprite->Set_Rotation_Affects_Rect(m_rotation_affects_rect);
    basic_sprite->Set_Scale_Affects_Rect(m_scale_affects_rect);
    basic_sprite->Set_Scale_Directions(m_scale_up, m_scale_down, m_scale_left, m_scale_right);
    basic_sprite->Set_Ignore_Camera(m_no_camera);
    basic_sprite->Set_Shadow_Pos(m_shadow_pos);
    basic_sprite->Set_Shadow_Color(m_shadow_color);
    basic_sprite->Set_Spawned(m_spawned);
    basic_sprite->Set_Suppress_Save(m_suppress_save);

    basic_sprite->m_uid = -1;

    return basic_sprite;
}
/**
 * This method saves the object into XML for saving it inside a level
 * XML file. Subclasses should override this *and* call the base class
 * method.  The subclasses should then add attributes to the node the
 * baseclass method returned and return that node themselves again.
 *
 * \param p_element libxml2’s node we are in currently.
 *
 * \return Exactly `p_element`.
 *
*/
xmlpp::Element* cSprite::Save_To_XML_Node(xmlpp::Element* p_element)
{

    xmlpp::Element* p_node = p_element->add_child_element(m_type_name);


    // position
    Add_Property(p_node, "posx", static_cast<int>(m_start_pos_x));
    Add_Property(p_node, "posy", static_cast<int>(m_start_pos_y));
    // UID
    Add_Property(p_node, "uid", m_uid);

    // image
    boost::filesystem::path img_filename;
    if (!m_image_filename.empty())
        img_filename = utf8_to_path(m_image_filename);
    else if (m_start_image)
        img_filename = m_start_image->m_path;
    else if (m_image)
        img_filename = m_image->m_path;
    else
        cerr << "Warnung: cSprite::Save_To_XML_Node() no image from type '" << m_type << "'" << endl;

    // Only save the relative part of the filename -- otherwise the
    // generated levels wouldn’t be portable.
    if (img_filename.is_absolute())
        img_filename = fs::relative(img_filename, pResource_Manager->Get_Game_Pixmaps_Directory());

    Add_Property(p_node, "image", img_filename.generic_string());

    // type (only if Get_XML_Type_Name() returns something meaningful)
    // type is massive type in real. Should probably have an own XML attribute.
    std::string type = Get_XML_Type_Name();
    if (type.empty())
        Add_Property(p_node, "type", Get_Massive_Type_Name(m_massive_type));
    else
        Add_Property(p_node, "type", type);

    return p_node;
}

/**
 * This method saves the sprite to the given savegame XML node.
 * It is intended to be called only inside the savegame mechanism.
 * It returns false by default, but still adds a "type" attribute
 * to the given XML element, so that in subclasses you can easily
 * override this method, add your own additional savegame attributes,
 * and return true to have the cSave_Level::Save_To_Node() method
 * consider the node for storing.
 *
 * "posx" and "posy" attributes for the initial position (m_start_pos*
 * attributes) are also saved.
 */
bool cSprite::Save_To_Savegame_XML_Node(xmlpp::Element* p_element) const
{
    Add_Property(p_element, "type", m_type);
    Add_Property(p_element, "posx", int_to_string(static_cast<int>(m_start_pos_x)));
    Add_Property(p_element, "posy", int_to_string(static_cast<int>(m_start_pos_y)));
    return false;
}

/**
 * This method specifies the image that is used for
 * drawing the sprite by default (you can override this
 * by overriding the Draw() method).
 *
 * \param new_image The image to switch to now.
 * \param new_start_image
 *   The default start_image will be set to the given image,
 *   i.e. not only the active image is changed, but the image
 *   to return to after changes. This is also the image shown
 *   in the editor.
 * \param del_img The given image will be deleted.
 */
void cSprite::Set_Image(cGL_Surface* new_image, bool new_start_image /* = 0 */, bool del_img /* = 0 */)
{
    if (m_delete_image) {
        if (m_image) {
            // if same image reset start_image
            if (m_start_image == m_image) {
                m_start_image = NULL;
            }

            delete m_image;
            m_image = NULL;
        }

        m_delete_image = 0;
    }

    m_image = new_image;

    if (m_image) {
        // collision data
        m_col_pos = m_image->m_col_pos;
        // scale affects the rect
        if (m_scale_affects_rect) {
            m_col_rect.m_w = m_image->m_col_w * m_scale_x;
            m_col_rect.m_h = m_image->m_col_h * m_scale_y;
            // image data
            m_rect.m_w = m_image->m_w * m_scale_x;
            m_rect.m_h = m_image->m_h * m_scale_y;
        }
        // scale does not affect the rect
        else {
            m_col_rect.m_w = m_image->m_col_w;
            m_col_rect.m_h = m_image->m_col_h;
            // image data
            m_rect.m_w = m_image->m_w;
            m_rect.m_h = m_image->m_h;
        }
        // rotation affects the rect
        if (m_rotation_affects_rect) {
            Update_Rect_Rotation();
        }

        m_delete_image = del_img;

        // if no name is set use the first image name
        if (m_name.empty()) {
            m_name = m_image->m_name;
        }
        // if no editor tags are set use the first image editor tags
        if (m_editor_tags.empty()) {
            m_editor_tags = m_image->m_editor_tags;
        }
    }
    else {
        // clear image data
        m_col_pos.m_x = 0.0f;
        m_col_pos.m_y = 0.0f;
        m_col_rect.m_w = 0.0f;
        m_col_rect.m_h = 0.0f;
        m_rect.m_w = 0.0f;
        m_rect.m_h = 0.0f;
    }

    if (!m_start_image || new_start_image) {
        m_start_image = new_image;

        if (m_start_image) {
            m_start_rect.m_w = m_start_image->m_w;
            m_start_rect.m_h = m_start_image->m_h;

            // always set the image name for basic sprites
            if (Is_Basic_Sprite()) {
                m_name = m_start_image->m_name;
            }
        }
        else {
            m_start_rect.m_w = 0.0f;
            m_start_rect.m_h = 0.0f;
        }
    }

    // because col_pos could have changed
    Update_Position_Rect();
}

void cSprite::Set_Sprite_Type(SpriteType type)
{
    m_type = type;
}

/**
 * Returns the string to use for the XML `type` property of
 * the sprite. Override in subclasses and do not call
 * the parent method. Returning an empty string causes
 * no `type` property to be written.
 */
std::string cSprite::Get_XML_Type_Name()
{
    if (m_sprite_array == ARRAY_UNDEFINED) {
        return "undefined";
    }
    else if (m_sprite_array == ARRAY_HUD) {
        return "hud";
    }
    else if (m_sprite_array == ARRAY_ANIM) {
        return "animation";
    }
    else {
        cerr << "Warning : Sprite unknown array " << m_sprite_array << endl;
    }

    return "";
}

void cSprite::Set_Ignore_Camera(bool enable /* = 0 */)
{
    // already set
    if (m_no_camera == enable) {
        return;
    }

    m_no_camera = enable;

    Update_Valid_Draw();
}

void cSprite::Set_Pos(float x, float y, bool new_startpos /* = 0 */)
{
    m_pos_x = x;
    m_pos_y = y;

    if (new_startpos || (Is_Float_Equal(m_start_pos_x, 0.0f) && Is_Float_Equal(m_start_pos_y, 0.0f))) {
        m_start_pos_x = x;
        m_start_pos_y = y;
    }

    Update_Position_Rect();
}

void cSprite::Set_Pos_X(float x, bool new_startpos /* = 0 */)
{
    m_pos_x = x;

    if (new_startpos) {
        m_start_pos_x = x;
    }

    Update_Position_Rect();
}

void cSprite::Set_Pos_Y(float y, bool new_startpos /* = 0 */)
{
    m_pos_y = y;

    if (new_startpos) {
        m_start_pos_y = y;
    }

    Update_Position_Rect();
}

void cSprite::Set_Active(bool enabled)
{
    // already set
    if (m_active == enabled) {
        return;
    }

    m_active = enabled;

    Update_Valid_Draw();
    Update_Valid_Update();
}

/** Set a Color Combination ( GL_ADD, GL_MODULATE or GL_REPLACE ).
 * Addition ( adds white to color )
 * 1.0 is the maximum and the given color will be white
 * 0.0 is the minimum and the color will have the default color
 * Modulation ( adds black to color )
 * 1.0 is the maximum and the color will have the default color
 * 0.0 is the minimum and the given color will be black
 * Replace ( replaces color value )
 * 1.0 is the maximum and the given color has maximum value
 * 0.0 is the minimum and the given color has minimum value
 */
void cSprite::Set_Color_Combine(const float red, const float green, const float blue, const GLint com_type)
{
    m_combine_type = com_type;
    m_combine_color[0] = Clamp(red, 0.000001f, 1.0f);
    m_combine_color[1] = Clamp(green, 0.000001f, 1.0f);
    m_combine_color[2] = Clamp(blue, 0.000001f, 1.0f);
}

void cSprite::Update_Rect_Rotation_Z(void)
{
    // rotate 270°
    if (m_rot_z >= 270.0f) {
        // rotate collision position
        float orig_x = m_col_pos.m_x;
        m_col_pos.m_x = m_col_pos.m_y;
        m_col_pos.m_y = orig_x;

        // switch width and height
        float orig_w = m_rect.m_w;
        m_rect.m_w = m_rect.m_h;
        m_rect.m_h = orig_w;
        // switch collision width and height
        float orig_col_w = m_col_rect.m_w;
        m_col_rect.m_w = m_col_rect.m_h;
        m_col_rect.m_h = orig_col_w;
    }
    // mirror
    else if (m_rot_z >= 180.0f) {
        m_col_pos.m_x = m_rect.m_w - (m_col_rect.m_w + m_col_pos.m_x);
        m_col_pos.m_y = m_rect.m_h - (m_col_rect.m_h + m_col_pos.m_y);
    }
    // rotate 90°
    else if (m_rot_z >= 0.00001f) {
        // rotate collision position
        float orig_x = m_col_pos.m_x;
        m_col_pos.m_x = m_rect.m_h - (m_col_rect.m_h + m_col_pos.m_y);
        m_col_pos.m_y = orig_x;

        // switch width and height
        float orig_w = m_rect.m_w;
        m_rect.m_w = m_rect.m_h;
        m_rect.m_h = orig_w;
        // switch collision width and height
        float orig_col_w = m_col_rect.m_w;
        m_col_rect.m_w = m_col_rect.m_h;
        m_col_rect.m_h = orig_col_w;
    }
}

void cSprite::Set_Rotation_X(float rot, bool new_start_rot /* = 0 */)
{
    m_rot_x = fmod(rot, 360.0f);

    if (new_start_rot) {
        m_start_rot_x = m_rot_x;
    }

    if (m_rotation_affects_rect) {
        Update_Rect_Rotation_X();
    }
}

void cSprite::Set_Rotation_Y(float rot, bool new_start_rot /* = 0 */)
{
    m_rot_y = fmod(rot, 360.0f);

    if (new_start_rot) {
        m_start_rot_y = m_rot_y;
    }

    if (m_rotation_affects_rect) {
        Update_Rect_Rotation_Y();
    }
}

void cSprite::Set_Rotation_Z(float rot, bool new_start_rot /* = 0 */)
{
    m_rot_z = fmod(rot, 360.0f);

    if (new_start_rot) {
        m_start_rot_z = m_rot_z;
    }

    if (m_rotation_affects_rect) {
        Update_Rect_Rotation_Z();
    }
}
void cSprite::Set_Scale_X(const float scale, const bool new_startscale /* = 0 */)
{
    // invalid value
    if (Is_Float_Equal(scale, 0.0f)) {
        return;
    }

    // undo previous scale from rect
    if (m_scale_affects_rect && m_scale_x != 1.0f) {
        m_col_rect.m_w /= m_scale_x;
        m_rect.m_w /= m_scale_x;
    }

    m_scale_x = scale;

    // set new scale to rect
    if (m_scale_affects_rect && m_scale_x != 1.0f) {
        m_col_rect.m_w *= m_scale_x;
        m_rect.m_w *= m_scale_x;
    }

    if (new_startscale) {
        m_start_scale_x = m_scale_x;
    }
}

void cSprite::Set_Scale_Y(const float scale, const bool new_startscale /* = 0 */)
{
    // invalid value
    if (Is_Float_Equal(scale, 0.0f)) {
        return;
    }

    // undo previous scale from rect
    if (m_scale_affects_rect && m_scale_y != 1.0f) {
        m_col_rect.m_h /= m_scale_y;
        m_rect.m_h /= m_scale_y;
    }

    m_scale_y = scale;

    // set new scale to rect
    if (m_scale_affects_rect && m_scale_y != 1.0f) {
        m_col_rect.m_h *= m_scale_y;
        m_rect.m_h *= m_scale_y;
    }

    if (new_startscale) {
        m_start_scale_y = m_scale_y;
    }
}
void cSprite::Set_On_Top(const cSprite* sprite, bool optimize_hor_pos /* = 1 */)
{
    // set ground position 0.1f over it
    m_pos_y = sprite->m_col_rect.m_y - m_col_pos.m_y - m_col_rect.m_h - 0.1f;

    // optimize the horizontal position if given
    if (optimize_hor_pos && (m_pos_x < sprite->m_pos_x || m_pos_x > sprite->m_pos_x + sprite->m_col_rect.m_w)) {
        m_pos_x = sprite->m_pos_x + sprite->m_col_rect.m_w / 3;
    }

    Update_Position_Rect();
}

void cSprite::Move(float move_x, float move_y, const bool real /* = 0 */)
{
    if (Is_Float_Equal(move_x, 0.0f) && Is_Float_Equal(move_y, 0.0f)) {
        return;
    }

    if (!real) {
        move_x *= pFramerate->m_speed_factor;
        move_y *= pFramerate->m_speed_factor;
    }

    m_pos_x += move_x;
    m_pos_y += move_y;

    Update_Position_Rect();
}

void cSprite::Update_Position_Rect(void)
{
    // if not editor mode
    if (!editor_enabled) {
        m_rect.m_x = m_pos_x;
        m_rect.m_y = m_pos_y;
        // editor rect
        m_start_rect.m_x = m_pos_x;
        m_start_rect.m_y = m_pos_y;
        // collision rect
        m_col_rect.m_x = m_pos_x + m_col_pos.m_x;
        m_col_rect.m_y = m_pos_y + m_col_pos.m_y;
    }
    // editor mode
    else {
        m_rect.m_x = m_start_pos_x;
        m_rect.m_y = m_start_pos_y;
        m_start_rect.m_x = m_start_pos_x;
        m_start_rect.m_y = m_start_pos_y;
        // Do not use m_start_pos_x/m_start_pos_y because col_rect is not the editor/start rect
        m_col_rect.m_x = m_pos_x + m_col_pos.m_x; // todo : startcol_pos ?
        m_col_rect.m_y = m_pos_y + m_col_pos.m_y;
    }

    Update_Valid_Draw();
}

void cSprite::Update_Valid_Draw(void)
{
    m_valid_draw = Is_Draw_Valid();
}

void cSprite::Update_Valid_Update(void)
{
    m_valid_update = Is_Update_Valid();
}

void cSprite::Draw(cSurface_Request* request /* = NULL */)
{
    if (!m_valid_draw) {
        return;
    }

    Draw_Image(request);

    // draw debugging collision rects
    if (game_debug) {
        // - image rect
        // create request
        cRect_Request* rect_request = new cRect_Request();
        // draw
        pVideo->Draw_Rect(&m_rect, m_pos_z + 0.000008f, &lightgrey, rect_request);
        rect_request->m_no_camera = m_no_camera;
        rect_request->m_filled = 0;
        rect_request->m_blend_sfactor = GL_SRC_COLOR;
        rect_request->m_blend_dfactor = GL_DST_ALPHA;
        // scale
        if (!m_scale_affects_rect) {
            // scale position y
            if (m_scale_up) {
                rect_request->m_rect.m_y += (m_image->m_int_y * m_scale_y) - ((m_image->m_h * 0.5f) * (m_scale_y - 1.0f));
            }

            // scale height
            if (m_scale_down) {
                rect_request->m_rect.m_h += m_image->m_h * (m_scale_y - 1.0f);
            }

            // scale position x
            if (m_scale_left) {
                rect_request->m_rect.m_x += (m_image->m_int_x * m_scale_x) - ((m_image->m_w * 0.5f) * (m_scale_x - 1.0f));
            }

            // scale width
            if (m_scale_right) {
                rect_request->m_rect.m_w += m_image->m_w * (m_scale_x - 1.0f);
            }
        }
        // add request
        pRenderer->Add(rect_request);

        // - collision rect
        // create request
        rect_request = new cRect_Request();
        // draw
        Color sprite_color = Get_Sprite_Color(this);
        pVideo->Draw_Rect(&m_col_rect, m_pos_z + 0.000007f, &sprite_color, rect_request);
        rect_request->m_no_camera = m_no_camera;
        // blending
        rect_request->m_blend_sfactor = GL_SRC_COLOR;
        rect_request->m_blend_dfactor = GL_DST_ALPHA;

        // add request
        pRenderer->Add(rect_request);
    }

    // show obsolete images in editor
    if (editor_enabled && m_image && m_image->m_obsolete) {
        // create request
        cRect_Request* rect_request = new cRect_Request();
        // draw
        pVideo->Draw_Rect(&m_col_rect, m_pos_z + 0.000005f, &red, rect_request);
        rect_request->m_no_camera = 0;

        // blending
        rect_request->m_blend_sfactor = GL_SRC_COLOR;
        rect_request->m_blend_dfactor = GL_DST_ALPHA;

        // add request
        pRenderer->Add(rect_request);
    }
}

void cSprite::Draw_Image(cSurface_Request* request /* = NULL */) const
{
    if (!m_valid_draw) {
        return;
    }

    bool create_request = 0;

    if (!request) {
        create_request = 1;
        // create request
        request = new cSurface_Request();
    }

    // editor
    if (editor_enabled) {
        Draw_Image_Editor(request);
    }
    // no editor
    else {
        Draw_Image_Normal(request);
    }

    if (create_request) {
        // add request
        pRenderer->Add(request);
    }
}

void cSprite::Draw_Image_Normal(cSurface_Request* request /* = NULL */) const
{
    // texture id
    request->m_texture_id = m_image->m_image;

    // size
    request->m_w = m_image->m_start_w;
    request->m_h = m_image->m_start_h;

    // rotation
    request->m_rot_x += m_rot_x + m_image->m_base_rot_x;
    request->m_rot_y += m_rot_y + m_image->m_base_rot_y;
    request->m_rot_z += m_rot_z + m_image->m_base_rot_z;

    // position x and
    // scalex
    if (m_scale_x != 1.0f) {
        // scale to the right and left
        if (m_scale_right && m_scale_left) {
            request->m_scale_x = m_scale_x;
            request->m_pos_x = m_pos_x + (m_image->m_int_x * m_scale_x) - ((m_image->m_w * 0.5f) * (m_scale_x - 1.0f));
        }
        // scale to the right only
        else if (m_scale_right) {
            request->m_scale_x = m_scale_x;
            request->m_pos_x = m_pos_x + (m_image->m_int_x * m_scale_x);
        }
        // scale to the left only
        else if (m_scale_left) {
            request->m_scale_x = m_scale_x;
            request->m_pos_x = m_pos_x + (m_image->m_int_x * m_scale_x) - ((m_image->m_w) * (m_scale_x - 1.0f));
        }
        // no scaling
        else {
            request->m_pos_x = m_pos_x + m_image->m_int_x;
        }
    }
    // no scalex
    else {
        request->m_pos_x = m_pos_x + m_image->m_int_x;
    }
    // position y and
    // scaley
    if (m_scale_y != 1.0f) {
        // scale down and up
        if (m_scale_down && m_scale_up) {
            request->m_scale_y = m_scale_y;
            request->m_pos_y = m_pos_y + (m_image->m_int_y * m_scale_y) - ((m_image->m_h * 0.5f) * (m_scale_y - 1.0f));
        }
        // scale down only
        else if (m_scale_down) {
            request->m_scale_y = m_scale_y;
            request->m_pos_y = m_pos_y + (m_image->m_int_y * m_scale_y);
        }
        // scale up only
        else if (m_scale_up) {
            request->m_scale_y = m_scale_y;
            request->m_pos_y = m_pos_y + (m_image->m_int_y * m_scale_y) - ((m_image->m_h) * (m_scale_y - 1.0f));
        }
        // no scaling
        else {
            request->m_pos_y = m_pos_y + m_image->m_int_y;
        }
    }
    // no scaley
    else {
        request->m_pos_y = m_pos_y + m_image->m_int_y;
    }

    // position z
    request->m_pos_z = m_pos_z;

    // no camera setting
    request->m_no_camera = m_no_camera;

    // color
    request->m_color = m_color;
    // combine color
    if (m_combine_type) {
        request->m_combine_type = m_combine_type;
        request->m_combine_color[0] = m_combine_color[0];
        request->m_combine_color[1] = m_combine_color[1];
        request->m_combine_color[2] = m_combine_color[2];
    }

    // shadow
    if (m_shadow_pos) {
        request->m_shadow_pos = m_shadow_pos;
        request->m_shadow_color = m_shadow_color;
    }
}

void cSprite::Draw_Image_Editor(cSurface_Request* request /* = NULL */) const
{
    // texture id
    request->m_texture_id = m_start_image->m_image;

    // size
    request->m_w = m_start_image->m_start_w;
    request->m_h = m_start_image->m_start_h;

    // rotation
    request->m_rot_x += m_start_rot_x + m_start_image->m_base_rot_x;
    request->m_rot_y += m_start_rot_y + m_start_image->m_base_rot_y;
    request->m_rot_z += m_start_rot_z + m_start_image->m_base_rot_z;

    // position x and
    // scalex
    if (m_start_scale_x != 1.0f) {
        // scale to the right and left
        if (m_scale_right && m_scale_left) {
            request->m_scale_x = m_start_scale_x;
            request->m_pos_x = m_start_pos_x + (m_start_image->m_int_x * m_start_scale_x) - ((m_start_image->m_w * 0.5f) * (m_start_scale_x - 1.0f));
        }
        // scale to the right only
        else if (m_scale_right) {
            request->m_scale_x = m_start_scale_x;
            request->m_pos_x = m_start_pos_x + (m_start_image->m_int_x * m_start_scale_x);
        }
        // scale to the left only
        else if (m_scale_left) {
            request->m_scale_x = m_start_scale_x;
            request->m_pos_x = m_start_pos_x + (m_start_image->m_int_x * m_start_scale_x) - ((m_start_image->m_w) * (m_start_scale_x - 1.0f));
        }
        // no scaling
        else {
            request->m_pos_x = m_start_pos_x + m_start_image->m_int_x;
        }
    }
    // no scalex
    else {
        request->m_pos_x = m_start_pos_x + m_start_image->m_int_x;
    }
    // position y and
    // scaley
    if (m_start_scale_y != 1.0f) {
        // scale down and up
        if (m_scale_down && m_scale_up) {
            request->m_scale_y = m_start_scale_y;
            request->m_pos_y = m_start_pos_y + (m_start_image->m_int_y * m_start_scale_y) - ((m_start_image->m_h * 0.5f) * (m_start_scale_y - 1.0f));
        }
        // scale down only
        else if (m_scale_down) {
            request->m_scale_y = m_start_scale_y;
            request->m_pos_y = m_start_pos_y + (m_start_image->m_int_y * m_start_scale_y);
        }
        // scale up only
        else if (m_scale_up) {
            request->m_scale_y = m_start_scale_y;
            request->m_pos_y = m_start_pos_y + (m_start_image->m_int_y * m_start_scale_y) - ((m_start_image->m_h) * (m_start_scale_y - 1.0f));
        }
        // no scaling
        else {
            request->m_pos_y = m_start_pos_y + m_start_image->m_int_y;
        }
    }
    // no scaley
    else {
        request->m_pos_y = m_start_pos_y + m_start_image->m_int_y;
    }

    // if editor z position is given
    if (m_editor_pos_z > 0.0f) {
        request->m_pos_z = m_editor_pos_z;
    }
    // normal position z
    else {
        request->m_pos_z = m_pos_z;
    }

    // no camera setting
    request->m_no_camera = m_no_camera;

    // color
    request->m_color = m_color;
    // combine color
    if (m_combine_type) {
        request->m_combine_type = m_combine_type;
        request->m_combine_color[0] = m_combine_color[0];
        request->m_combine_color[1] = m_combine_color[1];
        request->m_combine_color[2] = m_combine_color[2];
    }

    // shadow
    if (m_shadow_pos) {
        request->m_shadow_pos = m_shadow_pos;
        request->m_shadow_color = m_shadow_color;
    }
}

/**
 * - Should be called after setting the new array type.
 * - Overwrite this if you don’t want your object to me affected by "m"
 *   toggling in the level editor.
 */
void cSprite::Set_Massive_Type(MassiveType type)
{
    m_massive_type = type;

    // set massive-type z position
    if (m_massive_type == MASS_MASSIVE) {
        m_sprite_array = ARRAY_MASSIVE;
        m_pos_z = m_pos_z_massive_start;
        m_can_be_ground = true;
    }
    else if (m_massive_type == MASS_PASSIVE) {
        m_sprite_array = ARRAY_PASSIVE;
        m_pos_z = m_pos_z_passive_start;
        m_can_be_ground = false;
    }
    else if (m_massive_type == MASS_FRONT_PASSIVE) {
        m_sprite_array = ARRAY_PASSIVE;
        m_pos_z = m_pos_z_front_passive_start;
        m_can_be_ground = false;
    }
    else if (m_massive_type == MASS_HALFMASSIVE) {
        m_sprite_array = ARRAY_ACTIVE;
        m_pos_z = m_pos_z_halfmassive_start;
        m_can_be_ground = true;
    }
    else if (m_massive_type == MASS_CLIMBABLE) {
        m_sprite_array = ARRAY_ACTIVE;
        m_pos_z = m_pos_z_halfmassive_start;
        m_can_be_ground = false;
    }

    // make it the latest sprite
    m_sprite_manager->Move_To_Back(this);
}

bool cSprite::Is_On_Top(const cSprite* obj) const
{
    // invalid
    if (!obj) {
        return 0;
    }

    // always collide upwards because of the image size collision checking
    if (m_col_rect.m_x + m_col_rect.m_w > obj->m_col_rect.m_x && m_col_rect.m_x < obj->m_col_rect.m_x + obj->m_col_rect.m_w &&
            m_col_rect.m_y + m_col_rect.m_h < obj->m_col_rect.m_y) {
        return 1;
    }

    return 0;
}

bool cSprite::Is_Visible_On_Screen(void) const
{
    // camera position
    float cam_x = 0.0f;
    float cam_y = 0.0f;

    if (!m_no_camera) {
        cam_x = pActive_Camera->m_x;
        cam_y = pActive_Camera->m_y;
    }

    // not visible left
    if (m_rect.m_x + m_rect.m_w < cam_x) {
        return 0;
    }
    // not visible right
    else if (m_rect.m_x > cam_x + game_res_w) {
        return 0;
    }
    // not visible down
    else if (m_rect.m_y + m_rect.m_h < cam_y) {
        return 0;
    }
    // not visible up
    else if (m_rect.m_y > cam_y + game_res_h) {
        return 0;
    }

    return 1;
}

bool cSprite::Is_In_Range(void) const
{
    // no camera range set
    if (m_camera_range < 300) {
        return Is_Visible_On_Screen();
    }

    // check if not in range
    if (m_rect.m_x + (m_rect.m_w * 0.5f) < pActive_Camera->m_x + (game_res_w / 2) - m_camera_range ||
            m_rect.m_y + (m_rect.m_h * 0.5f) < pActive_Camera->m_y + (game_res_h / 2) - m_camera_range ||
            m_rect.m_x + (m_rect.m_w * 0.5f) > pActive_Camera->m_x + (game_res_w / 2) + m_camera_range ||
            m_rect.m_y + (m_rect.m_h * 0.5f) > pActive_Camera->m_y + (game_res_h / 2) + m_camera_range) {
        return 0;
    }

    // is in range
    return 1;
}

bool cSprite::Is_Update_Valid()
{
    // if destroyed
    if (m_auto_destroy) {
        return 0;
    }

    return 1;
}

bool cSprite::Is_Draw_Valid(void)
{
    // if editor not enabled
    if (!editor_enabled) {
        // if not active or no image is set
        if (!m_active || !m_image) {
            return 0;
        }
    }
    // editor enabled
    else {
        // if destroyed
        if (m_auto_destroy) {
            return 0;
        }

        // no image
        if (!m_start_image) {
            return 0;
        }
    }

    // not visible on the screen
    if (!Is_Visible_On_Screen()) {
        return 0;
    }

    return 1;
}

void cSprite::Destroy(void)
{
    // already destroyed
    if (m_auto_destroy) {
        return;
    }

    Clear_Collisions();

    // if this can not be auto-deleted
    if (m_disallow_managed_delete) {
        return;
    }

    m_auto_destroy = 1;
    m_active = 0;
    m_valid_draw = 0;
    m_valid_update = 0;
    Set_Image(NULL, 1);
}

/**
 * Configure the object configuration panel (the editor panel on the right
 * side) for this object. This method must be overridden in subclasses
 * *without* calling the parent cSprite::Editor_Active(), because in this
 * class it implements the behaviour for basic sprites and only those.
 * In your override, call Editor_Init() at the end.
 *
 * Use cEditor::Add_Config_Widget() to populate the configuration panel.
 *
 * This hook method is called when the user double-clicks an object
 * in the editor.
 */
void cSprite::Editor_Activate(void)
{
    // if this is not a basic sprite
    if (!Is_Basic_Sprite()) {
        return;
    }

    // get window manager
    CEGUI::WindowManager& wmgr = CEGUI::WindowManager::getSingleton();

    // image
    CEGUI::Editbox* editbox = static_cast<CEGUI::Editbox*>(wmgr.createWindow("TSCLook256/Editbox", "editor_sprite_image"));

    // Find active editor
    cEditor* p_editor = NULL;

    if (pLevel_Editor->m_enabled)
        p_editor = pLevel_Editor;
    else if (pWorld_Editor->m_enabled)
        p_editor = pWorld_Editor;
    else
        throw(std::runtime_error("Unknown editing environment"));

    p_editor->Add_Config_Widget(UTF8_("Image"), UTF8_("Image filename"), editbox);

    fs::path rel = fs::relative(m_start_image->Get_Path(), pResource_Manager->Get_Game_Pixmaps_Directory());
    editbox->setText(path_to_utf8(rel));
    editbox->subscribeEvent(CEGUI::Editbox::EventTextChanged, CEGUI::Event::Subscriber(&cSprite::Editor_Image_Text_Changed, this));

    // init
    Editor_Init();
}

/**
 * What to do when the user leaves object editing mode.
 * By default, this method just calls cEditor::Hide_Config_Panel()
 * on the active editor.
 */
void cSprite::Editor_Deactivate(void)
{
    if (pLevel_Editor->m_enabled)
        pLevel_Editor->Hide_Config_Panel();
    else if (pWorld_Editor->m_enabled)
        pWorld_Editor->Hide_Config_Panel();
    else
        throw(std::runtime_error("Unknown editing environment"));
}

/**
 * Calls the Editor_State_Update() hook method and then displays the
 * active editor's (level or world editor's) object configuration
 * panel.
 *
 * You have to call this method at the end of Editor_Activate().
 *
 * Do not override this method.
 */
void cSprite::Editor_Init()
{
    // set state
    Editor_State_Update();

    // Show
    if (pLevel_Editor->m_enabled)
        pLevel_Editor->Show_Config_Panel();
    else if (pWorld_Editor->m_enabled)
        pWorld_Editor->Show_Config_Panel();
    else
        throw(std::runtime_error("Unknown editing environment"));
}

bool cSprite::Editor_Image_Text_Changed(const CEGUI::EventArgs& event)
{
    const CEGUI::WindowEventArgs& windowEventArgs = static_cast<const CEGUI::WindowEventArgs&>(event);
    std::string str_text = static_cast<CEGUI::Editbox*>(windowEventArgs.window)->getText().c_str();

    Set_Image(pVideo->Get_Surface(utf8_to_path(str_text)), true);       // Automatically converted to absolute path by Get_Surface()

    return 1;
}

/**
 * This method should append all necessary components
 * to m_name and return the result as a new string.
 * This is how the object is presented to the user
 * in the editor. By default it just returns `m_name`.
 */
std::string cSprite::Create_Name() const
{
    return m_name;
}

/* *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** */

} // namespace TSC
