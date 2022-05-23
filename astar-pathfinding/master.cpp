#include <boost/graph/astar_search.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/grid_graph.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include<SFML/Graphics.hpp>

typedef boost::grid_graph<2> grid;
typedef boost::graph_traits<grid>::vertex_descriptor vertex_descriptor;
typedef boost::graph_traits<grid>::vertices_size_type vertices_size_type;

//���-������� ��� ������
struct vertex_hash
{
    size_t operator()(const vertex_descriptor& descriptor) const
    {
        size_t seed = 0;
        boost::hash_combine(seed, descriptor[0]); //���������� ���-�������� � � �
        boost::hash_combine(seed, descriptor[1]);
        return seed;
    }
};

typedef boost::unordered_set<vertex_descriptor, vertex_hash> vertex_set; //��������� ������ ��� �������� ������, ������ ���������� ��������
//���������� �� ������ ���-�������
typedef boost::vertex_subset_complement_filter<grid, vertex_set>::type filtered_grid;//���� ��� ���������� ������
//"�������" �������� �������, �������� ����� ����
class field
{
private:
    grid underlying_grid; //��������������� �����
    vertex_set barriers; //�������
    filtered_grid main_grid; //�������� �����
    vertex_set shortest_path; //�������, ���������� ���������� ����

    vertex_descriptor source; //����� ������
    vertex_descriptor goal; //������� �����

    bool need_update; //��� ��������� ����� ������/���� ���������� �������� �������� � ����������� ����
    //������ ��������� ��������������� ����� ��������� �����������
    grid create_grid(size_t x, size_t y)
    {
        boost::array<size_t, 2> lengths = { {x, y} };
        return grid(lengths);
    }
    bool has_barrier(vertex_descriptor vertex) const //���� ������ �� �������, �� ���������� true
    {
        return barriers.find(vertex) != barriers.end();
    }
    //��������� ��������������� ����� � �������� ��������
    filtered_grid create_barrier_grid()
    {
        return boost::make_vertex_subset_complement_filter(underlying_grid, barriers);
    }

public:
    //���������� need_update = true �� ��� ����� ������� �������
    field() : underlying_grid(create_grid(0, 0)), main_grid(create_barrier_grid()) { need_update = true;  source = vertex(0, underlying_grid), goal = vertex(num_vertices(underlying_grid) - 1, underlying_grid); };
    field(size_t x, size_t y) : underlying_grid(create_grid(x, y)), main_grid(create_barrier_grid()) { need_update = true;  source = vertex(0, underlying_grid), goal = vertex(num_vertices(underlying_grid) - 1, underlying_grid); };

    vertex_descriptor get_source() const { return source; }
    vertex_descriptor get_goal() const { return goal; }

    void set_source(const vertex_descriptor& src) { source = src; }
    void set_goal(const vertex_descriptor& g) { goal = g; }

    bool solve(); //�������� ������� ������ ����
    bool solved() const { return !shortest_path.empty(); }
    bool solution_contains(vertex_descriptor vertex) const
    {
        return shortest_path.find(vertex) != shortest_path.end();
    }
    void update(); //��������� ��������
    void draw(); //������ ��������
    friend void random_barriers(field&);
};

//��������� ������� ��� ���������� ����� ������� �������� � �����
class euclidean_heuristic :public boost::astar_heuristic<filtered_grid, double>
{
private:
    vertex_descriptor goal;
public:
    euclidean_heuristic(vertex_descriptor g) :goal(g) {};
    double operator()(vertex_descriptor vertex)
    {
        double x = goal[0] - vertex[0];
        double y = goal[1] - vertex[1];
        return sqrt(x * x + y * y);
    }
};

//���������� ����������, ���� ����� ����
struct found_goal {};

//�����-����������, ��� ����� examine_vertex ���������� ����������
struct astar_goal_visitor : public boost::default_astar_visitor
{
private:
    vertex_descriptor goal;
public:
    astar_goal_visitor(vertex_descriptor g) : goal(g) {};
    void examine_vertex(vertex_descriptor vertex, const filtered_grid&)
    {
        if (vertex == goal)
            throw found_goal();
    }
};

//������� ����-�����. ���������� true, ���� ����� ����
bool field::solve()
{
    typedef boost::unordered_map<vertex_descriptor, double, vertex_hash> dist_map;
    typedef boost::unordered_map<vertex_descriptor, vertex_descriptor, vertex_hash> pred_map;

    boost::static_property_map<double> weight(1); //���������� ����� �������� �����
    pred_map predecessor; //���������������
    boost::associative_property_map<pred_map> pred_pmap(predecessor);
    dist_map distance; //����� ����������� ����������
    boost::associative_property_map<dist_map> dist_pmap(distance);

    vertex_descriptor src = get_source();
    vertex_descriptor g = get_goal();
    euclidean_heuristic heuristic(g);
    astar_goal_visitor visitor(g);
    shortest_path.clear(); //������������ ��������, ����� ���� �� ������������� ���� �� �����
    try
    {//����� ���� � ��������, ������� ��������� ������ �� ����-�����, ���������� �������� �����, �������
        //�� predecessor ����������� ��������� � ���������� distance
        astar_search(main_grid, src, heuristic,
            boost::weight_map(weight)
            .predecessor_map(pred_pmap)
            .distance_map(dist_pmap)
            .visitor(visitor));
    }
    catch (found_goal fg)
    {
        //��� � �������� ����������� �� ���� ����� ������� ����������������, ��������
        //������� � ���� �������
        for (vertex_descriptor vertex = g; vertex != src; vertex = predecessor[vertex])
            shortest_path.insert(vertex);
        shortest_path.insert(src);
        return true;
    }
    return false;
}

//������� ����
constexpr float window_width = 900;
constexpr float window_height = 450;

sf::RenderWindow window(sf::VideoMode(window_width, window_height), "A* Pathfinder", sf::Style::Close);
sf::Event evnt;

const sf::Vector2f tile_size = { 45, 45 }; //������ ������
const size_t collumns_x = window_width / tile_size.x;
const size_t collumns_y = window_height / tile_size.y;

//����� �������
const sf::Color outer_border_color = sf::Color::Black;
const sf::Color source_color = sf::Color::Blue;
const sf::Color goal_color = sf::Color::Red;
const sf::Color path_color = sf::Color::Green;
const sf::Color barrier_color = sf::Color::Black;

void field::update()
{
    //���������� �����
    int x = sf::Mouse::getPosition(window).x / tile_size.x;
    int y = sf::Mouse::getPosition(window).y / tile_size.y;

    if (x >= 0 && x < collumns_x && y >= 0 && y < collumns_y)
    {
        vertex_descriptor coordinates = { { x, vertices_size_type(y) } }; //����������� � ������ ��� ��� ���������
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
        {//������ ��������� ��������� �� ����� ������ ����
            if (get_goal() != coordinates and !has_barrier(coordinates)) //����� �� ������� ��������� ������ � ���� � �� �� ������ � �������
            {
                set_source(coordinates);
                need_update = true; //����� �������� ��������
            }
        }
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right))
        {//������ ��������� ���� �� ����� ������ ����
            if (get_source() != coordinates and !has_barrier(coordinates))
            {
                set_goal(coordinates);
                need_update = true;
            }
        }
    }
    if (need_update) //���� ����� �������� ��������
    {
        solve(); //������ ������� ����
        need_update = false; //��� �� �����
    }
}

void field::draw()
{
    for (size_t x = 0; x < collumns_x; x++)
    {
        for (size_t y = 0; y < collumns_y; y++)
        {
            vertex_descriptor coordinates = { {x, vertices_size_type(y)} }; //���������� ������ ���������

            sf::RectangleShape first; //������ 
            sf::RectangleShape second;
            first.setSize(tile_size); //������
            first.setPosition(x * tile_size.x, y * tile_size.y); //�������������� �� ����������� ����
            first.setOutlineThickness(tile_size.x / 30); //�������
            first.setOutlineColor(outer_border_color); //���� ������� ������

            if (coordinates == get_goal()) //���� ��� ����, �� ������� ����
                first.setFillColor(goal_color);
            else if (coordinates == get_source()) //���� ������, �� �����
                first.setFillColor(source_color);
            else if (has_barrier(coordinates)) //���� � ������ ���� ������, �� ������
                first.setFillColor(barrier_color);

            second.setOutlineThickness(0); //��� ���� ������� ������� ������
            if (solution_contains(coordinates) and coordinates != get_source() and coordinates != get_goal())
            { //���� ������ �� �������� ������/����� ����, ��
                second.setFillColor(path_color); //������ ����
                second.setSize({ tile_size.x / 2, tile_size.y / 2 }); //� ������ ��������
                second.setPosition(x * tile_size.x + tile_size.x / 4, y * tile_size.y + tile_size.y / 4); //��������������� ������������
            }
            //������������
            window.draw(first);
            window.draw(second);
        }
    }
}

void random_barriers(field& f) //��������� ���������� ��������
{
    vertices_size_type n = num_vertices(f.underlying_grid);
    vertex_descriptor src = f.get_source();
    vertex_descriptor g = f.get_goal();
    int barriers = n / 4; //�������� �������� ������������
    while (barriers > 0)
    {
        size_t direction = rand() % 2; //�������������� ��� ������������
        vertices_size_type wall = rand() % 4;
        vertex_descriptor vert = vertex(rand() % (n - 1), f.underlying_grid);
        while (wall)
        {
            //� ��������� � �������� ����� �� ����� ���� ��������
            if (vert != src and vert != g)
            {
                wall--;
                if (!f.has_barrier(vert)) //���� ��� �������
                {
                    f.barriers.insert(vert); //�� �����
                    barriers--; //��������� ����� �����
                }
            }
            vertex_descriptor next_vert = f.underlying_grid.next(vert, direction); //��������� ������� � ���� �����������
            if (vert == next_vert)//������������� �������� ����� ��� ���������� ���� ����            
                break;
            vert = next_vert;
        }
    }
}

int main()
{
    //��������� ����, ��� ������ ��� 20 �� 10

    srand(time(NULL));
    field f(collumns_x, collumns_y); //������ ����
    random_barriers(f); //��������� �������
    window.setFramerateLimit(60);

    while (window.isOpen())
    {
        while (window.pollEvent(evnt)) //�������� ����
            if (evnt.type == sf::Event::Closed) window.close();
        f.update();
        window.clear(sf::Color::White);
        f.draw();
        window.display();
    }
    return 0;
}

