// Copyright (c) 2014-2019 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#include <cassert>
#include <sstream>
#include <vector>

#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/change_state.hpp>
#include <tao/pegtl/contrib/json.hpp>

#include "json_classes.hpp"
#include "json_errors.hpp"
#include "json_unescape.hpp"

namespace pegtl = TAO_PEGTL_NAMESPACE;

namespace examples
{
   // State class that stores the result of a JSON parsing run -- a single JSON object.
   // The other members are used temporarily, at the end of a (successful) parsing run.
   // They are expected to be empty.

   struct json_state
   {
      std::shared_ptr< json_base > result;
      std::vector< std::string > keys;
      std::vector< std::shared_ptr< array_json > > arrays;
      std::vector< std::shared_ptr< object_json > > objects;
   };

   // Action class

   template< typename Rule >
   struct action : unescape_action< Rule >  // Inherit from json_unescape.hpp.
   {
   };

   template<>
   struct action< pegtl::json::null >
   {
      static void apply0( json_state& state )
      {
         state.result = std::make_shared< null_json >();
      }
   };

   template<>
   struct action< pegtl::json::true_ >
   {
      static void apply0( json_state& state )
      {
         state.result = std::make_shared< boolean_json >( true );
      }
   };

   template<>
   struct action< pegtl::json::false_ >
   {
      static void apply0( json_state& state )
      {
         state.result = std::make_shared< boolean_json >( false );
      }
   };

   template<>
   struct action< pegtl::json::number >
   {
      template< typename Input >
      static void apply( const Input& in, json_state& state )
      {
         std::stringstream ss;
         ss << in.string();
         long double v;
         ss >> v;  // NOTE: not quite correct for JSON but we'll use it for this simple example.
         state.result = std::make_shared< number_json >( v );
      }
   };

   // To parse a string, we change the state to decouple string parsing/unescaping

   struct string_state
      : public unescape_state_base
   {
      template< typename Input, typename... States >
      explicit string_state( const Input& /*unused*/, States&&... /*unused*/ ) noexcept
      {
      }

      template< typename Input >
      void success( const Input& /*unused*/, json_state& state )
      {
         state.result = std::make_shared< string_json >( unescaped );
      }
   };

   template<>
   struct action< pegtl::json::string::content >
      : pegtl::change_state< string_state >
   {
   };

   template<>
   struct action< pegtl::json::array::begin >
   {
      static void apply0( json_state& state )
      {
         state.arrays.push_back( std::make_shared< array_json >() );
      }
   };

   template<>
   struct action< pegtl::json::array::element >
   {
      static void apply0( json_state& state )
      {
         state.arrays.back()->data.push_back( std::move( state.result ) );
      }
   };

   template<>
   struct action< pegtl::json::array::end >
   {
      static void apply0( json_state& state )
      {
         state.result = std::move( state.arrays.back() );
         state.arrays.pop_back();
      }
   };

   template<>
   struct action< pegtl::json::object::begin >
   {
      static void apply0( json_state& state )
      {
         state.objects.push_back( std::make_shared< object_json >() );
      }
   };

   // To parse a key, we change the state to decouple string parsing/unescaping

   struct key_state : unescape_state_base
   {
      template< typename Input, typename... States >
      explicit key_state( const Input& /*unused*/, States&&... /*unused*/ ) noexcept
      {
      }

      template< typename Input >
      void success( const Input& /*unused*/, json_state& state )
      {
         state.keys.push_back( std::move( unescaped ) );
      }
   };

   template<>
   struct action< pegtl::json::key::content >
      : pegtl::change_state< key_state >
   {
   };

   template<>
   struct action< pegtl::json::object::element >
   {
      static void apply0( json_state& state )
      {
         state.objects.back()->data[ std::move( state.keys.back() ) ] = std::move( state.result );
         state.keys.pop_back();
      }
   };

   template<>
   struct action< pegtl::json::object::end >
   {
      static void apply0( json_state& state )
      {
         state.result = std::move( state.objects.back() );
         state.objects.pop_back();
      }
   };

   using grammar = pegtl::must< pegtl::json::text, pegtl::eof >;

}  // namespace examples

int main( int argc, char** argv )
{
   if( argc != 2 ) {
      std::cerr << "usage: " << argv[ 0 ] << " <json>";
   }
   else {
      examples::json_state state;
      pegtl::file_input in( argv[ 1 ] );
      pegtl::parse< examples::grammar, examples::action, examples::errors >( in, state );
      assert( state.keys.empty() );
      assert( state.arrays.empty() );
      assert( state.objects.empty() );
      std::cout << state.result << std::endl;
   }
   return 0;
}
