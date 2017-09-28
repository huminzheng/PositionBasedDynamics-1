//
// Created by Jack Purvis
//

#ifndef POSITIONBASEDDYNAMICS_MESH_HPP
#define POSITIONBASEDDYNAMICS_MESH_HPP

#include <set>
#include <string>
#include <vector>
#include <Eigen>
#include <camera.hpp>
#include <GL/glew.h>

using namespace std;
using namespace Eigen;

struct Vertex {
    int p;
    int t;
    int n;
};

struct Edge {
    Edge(Vertex vertexA, Vertex vertexB) {
        v[0] = vertexA;
        v[1] = vertexB;
    }
    Vertex v[2];
};

struct EdgeCompare {
    std::less<std::pair<int, int>> lessComparator;
    bool operator() (const Edge &l, const Edge &r) {
        return lessComparator(minmax(l.v[0].p, l.v[1].p), minmax(r.v[0].p, r.v[1].p));
    }
};

struct Triangle {
    Vertex v[3];
};

enum ConstraintType {
    FIXED,
    DISTANCE
};

struct Constraint {
    int cardinatlity;
    vector<int> indices;
    ConstraintType type;
    Vector3f target;
    float distance = 0.0f;
    float stiffness = 1.0f;
};

class Mesh {

public:
    Mesh(string filename, Vector3f colour);
    void reset();
    void render(Camera* camera, Matrix4f transform);

    int numVertices;
    int numFaces;

    Vector3f position = Vector3f(0.0f, 0.0f, 0.0f);

    // Mesh fields
    vector<Vector3f> initialVertices;
    vector<Vector3f> vertices;
    vector<Vector2f> uvs;
    vector<Vector3f> normals;
    set<Edge, EdgeCompare> edges;
    vector<Triangle> triangles;
    std::vector<Vector3f> surfaceNormals;

    // Simulation fields
    vector<Vector3f> velocities;
    vector<float> inverseMasses;
    vector<Vector3f> estimatePositions;
    vector<Constraint> constraints;
    bool gravityAffected = false;
    bool windAffected = false;

private:
    void parseObjFile(string filename);
    void generateSurfaceNormals();

    // VBOs
    GLuint positionVBO;
    GLuint normalVBO;

    // Rendering
    GLuint shader;
    Vector3f colour;

};

#endif //POSITIONBASEDDYNAMICS_MESH_HPP
