#include "../SceneNodes.h"


#pragma region "Base Node"

void BaseNode::Adopt( SceneNode* child )
{
    child->m_Parent = this;
    m_Children.push_back( child );
    child->Cascade_Data();
}

void BaseNode::OrphanSelf()
{
    if ( m_Parent )
    {
        auto &p_children = m_Parent->m_Children;
        m_Parent = nullptr;
        for ( auto it = p_children.begin(); it != p_children.end(); ++it )
        {
            if ( *it == this )
            {
                p_children.erase( it );
                return;
            }
        }
    }
    Cascade_Data();
}

void BaseNode::Flatten_SubGraph()
{
    if ( m_Children.size() != 0 )
    {
        using ChildList = std::vector<SceneNode*>&;
        ChildList KILL_LIST = m_Children;

        auto it = KILL_LIST.begin();
        unsigned int i = 0;
        ///Gnome Routine
        while ( it != KILL_LIST.end() )
        {
            it = KILL_LIST.begin() + i++;
            ( *it )->m_Parent = this;
            ChildList children = ( *it )->m_Children;

            KILL_LIST.reserve( children.size() + KILL_LIST.size() );
            KILL_LIST.insert( KILL_LIST.end(), children.begin(), children.end() );
            children.clear();
        }
    }
}

void BaseNode::Kill_SubGraph()
{
    if ( m_Children.size() != 0 )
    {
        using ChildList = std::vector<SceneNode*>&;
        ChildList KILL_LIST = m_Children;

        auto it = KILL_LIST.begin();
        unsigned int i = 0;
        ///Gnome Routine
        while ( it != KILL_LIST.end() )
        {
            it = KILL_LIST.begin() + i++;
            ( *it )->m_Parent = nullptr;
            ChildList children = ( *it )->m_Children;

            KILL_LIST.reserve( children.size() + KILL_LIST.size() );
            KILL_LIST.insert( KILL_LIST.end(), children.begin(), children.end() );
            children.clear();

            delete ( *it );
        }
    }
    m_Children.clear();
}

#pragma endregion


#pragma region "RootNode"

void RootNode::Cascade_Data()
{
    for ( auto C : m_Children )
    {
        C->Cascade_Data();
    }
}

void RootNode::Draw()
{
    m_Mutex.lock();
    for ( auto C : m_Children )
    {
        C->Draw();
    }
    m_Mutex.unlock();
}

void RootNode::Update( double& seconds )
{
    double minutes = seconds / 60.0f;
    m_Mutex.lock();
    for ( auto C : m_Children )
    {
        C->Update( minutes );
    }
    m_Mutex.unlock();
}

RootNode::~RootNode()
{
    Kill_SubGraph();
}

#pragma endregion


#pragma region "SceneNode"

#pragma region "BaseNode Overrides"
//final override
void SceneNode::Cascade_Data()
{
    Update_ModelMatrix();
    for ( auto C : m_Children )
    {
        C->Cascade_Data();
    }
}

//final override
void SceneNode::Draw()
{
    if ( m_Drawable )
    {
        m_Drawable->Draw( m_ModelMatrix );
    }
    for ( auto C : m_Children )
    {
        C->Draw();
    }
}

//override
void SceneNode::Update( double& minutes )
{
    Update_ModelMatrix();
    for ( auto C : m_Children )
    {
        C->Update( minutes );
    }
}
#pragma endregion

//virtual
void SceneNode::Update_ModelMatrix()
{
    Copy_ParentMatrix();
}

void SceneNode::Copy_ParentMatrix()
{
    if ( m_Parent != nullptr )
        m_ModelMatrix = m_Parent->m_ModelMatrix;
    else
        m_ModelMatrix = glm::mat4( 1.f );
}

SceneNode::SceneNode( BaseNode* parent )
{
    parent->Adopt( this );
    Copy_ParentMatrix();
}

SceneNode::~SceneNode()
{
    Kill_SubGraph();
    if ( m_Parent )
    {
        for ( auto it = m_Parent->m_Children.begin(); it != m_Parent->m_Children.end(); ++it )
        {
            if ( this == *it )
            {
                m_Parent->m_Children.erase( it );
                break;
            }
        }
    }
}

#pragma endregion


#pragma region "Transform Node"

TranNode::TranNode( BaseNode* parent ) : SceneNode( parent ){}
TranNode::TranNode( BaseNode* parent, glm::vec3& axis ) : SceneNode( parent )
{
    m_Axis = axis;
}

void TranNode::Update_ModelMatrix()
{
    Copy_ParentMatrix();
    m_ModelMatrix = m_ModelMatrix * m_TMatrix;
}

void TranNode::CheapMove( const glm::vec3& displacement )
{
    m_TMatrix = glm::translate( m_TMatrix, displacement );
}

void TranNode::Update( double& minutes )
{
    if ( m_Speed > 0.0001f || m_Speed < -0.0001f )
    {
        CheapMove( (float)( minutes * m_Speed ) * m_Axis );
    }
    SceneNode::Update( minutes );
}

void TranNode::Move( float& distance )
{
    CheapMove( distance * m_Axis );
    Cascade_Data();
}

void TranNode::Move( const glm::vec3& displacement )
{
    CheapMove( displacement );
    Cascade_Data();
}

void TranNode::SetAxis( const glm::vec3& axis )
{
    m_Axis = axis;
}

void TranNode::SetSpeed( float& speed )
{
    m_Speed = speed;
}

void TranNode::SetVelocity( const glm::vec3& velocity )
{
    m_Speed = glm::length( velocity );
    m_Axis = velocity / m_Speed;
}

#pragma endregion


#pragma region "Rotator Node"

RotNode::RotNode( BaseNode* parent ) : SceneNode( parent ){}
RotNode::RotNode( BaseNode* parent, glm::quat& rpm ) : SceneNode( parent )
{
    m_RPM = rpm;
}

void RotNode::Update_ModelMatrix()
{
    Copy_ParentMatrix();
    m_ModelMatrix = m_ModelMatrix * m_RMatrix;
}

void RotNode::CheapRotate( const glm::quat& rot )
{
    m_RMatrix = glm::mat4_cast( glm::quat_cast( m_RMatrix )*rot );
}

void RotNode::Update( double& minutes )
{
    static glm::quat EmptyQuat;
    if ( m_RPM != EmptyQuat )
    {
        CheapRotate( (float)minutes * m_RPM );
    }
    SceneNode::Update( minutes );
}

void RotNode::Rotate( const glm::quat& rot )
{
    CheapRotate( rot );
    Cascade_Data();
}

void RotNode::Rotate( const glm::vec3& axis, float& rot )
{
    glm::quat q;
    float r = sin( rot / 2 );
    q.x = axis.x * r;
    q.y = axis.y * r;
    q.z = axis.z * r;
    q.w = cos( rot / 2 );
    CheapRotate( q );
    Cascade_Data();
}

void RotNode::SetRPM( const glm::quat& rpm )
{
    m_RPM = rpm;
}

void RotNode::SetRPM( const glm::vec3& axis, float& rot )
{
    float r = sin( rot / 2 );
    m_RPM.x = axis.x * r;
    m_RPM.y = axis.y * r;
    m_RPM.z = axis.z * r;
    m_RPM.w = cos( rot / 2 );
}

#pragma endregion


#pragma region "Scaler Node"

ScalerNode::ScalerNode( BaseNode* parent ) : SceneNode( parent ){}
ScalerNode::ScalerNode( BaseNode* parent, glm::vec3& spm ) : SceneNode( parent )
{
    m_SPM = spm;
}

void ScalerNode::Update_ModelMatrix()
{
    Copy_ParentMatrix();
    m_ModelMatrix = m_ModelMatrix * m_SMatrix;
}

void ScalerNode::CheapScale( const glm::vec3& scale )
{
    m_SMatrix = glm::scale( m_SMatrix, scale );
}

void ScalerNode::Update( double& minutes )
{
    if ( m_SPM != glm::vec3() )
    {
        CheapScale( m_SPM );
    }
    SceneNode::Update( minutes );
}

void ScalerNode::Scale( const glm::vec3& scale )
{
    CheapScale( scale );
    Cascade_Data();
}

void ScalerNode::SetSPM( const glm::vec3& spm )
{
    m_SPM = spm;
}

#pragma endregion